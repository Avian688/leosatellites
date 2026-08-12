# Endpoint K-Shortest Path Snapshots

`LeoIpv4NetworkConfigurator` can generate and load companion K-shortest path
snapshots for selected end-to-end host pairs. Normal IP forwarding continues to
use the primary route snapshots. Satellite-to-satellite and ground-station-to-
ground-station K paths are never generated unless those modules are represented
by actual end hosts outside the routable core.

The companion files remove Yen calculations from normal protocol runs. They do
not yet assign MPTCP/MPORB subflows to paths or force packets to follow a saved
node sequence.

## Generate A Path Set

Use the endpoint geometry for the experiment, load the existing primary routing
corpus, and generate one named companion set:

```ini
**.loadFiles = true
*.configurator.numOfKPaths = 5
*.configurator.kPathMaxRttSpread = 5ms
*.configurator.kPathsEdgeDisjoint = false

*.configurator.kPathSnapshotMode = "generate"
*.configurator.kPathSnapshotSet = "SanDiegoToSeattle_ISL"
*.configurator.kPathEndpointPairs = "userTerminal[0]->userTerminal[1]"
*.configurator.allowRouteSnapshotOverwrite = true
```

`loadFiles=true` is intentional: the run reuses the primary delta route corpus
and computes only the configured endpoint K paths. Generation also works with
`loadFiles=false`, in which case primary and companion snapshots are written in
the same run.

For a topology where every end host needs to communicate with every other end
host, use:

```ini
*.configurator.kPathEndpointPairs = "all"
```

`all` means all unordered endpoint pairs among clients, servers, and user
terminals. It never means all satellites or ground stations. Explicit pair names
are preferable when only a subset carries multipath traffic.

## Load A Path Set

Normal MPTCP/MPORB runs use the same endpoint geometry, path policy, pair list,
and set name:

```ini
**.loadFiles = true
*.configurator.numOfKPaths = 5
*.configurator.kPathMaxRttSpread = 5ms
*.configurator.kPathsEdgeDisjoint = false

*.configurator.kPathSnapshotMode = "load"
*.configurator.kPathSnapshotSet = "SanDiegoToSeattle_ISL"
*.configurator.kPathEndpointPairs = "userTerminal[0]->userTerminal[1]"
```

Load mode performs no path calculation. It validates and indexes the complete
file before replacing the current catalog.

## Corpus Layout

Companion snapshots live below the existing constellation directory without
changing primary route filenames:

```text
1584_550_72_22_53_100_ISL/
  0.bin
  0.100001.bin
  ...
  kpaths/
    SanDiegoToSeattle_ISL/
      v2-yen-k5-rtt5ms-pairs-0123456789abcdef/
        0.bin
        0.100001.bin
        ...
```

The final profile directory is generated automatically. It encodes the format
version, solver, edge-disjoint policy, maximum K, exact RTT-spread value, and a
stable hash of the configured endpoint-pair set. Changing any path-selection
parameter therefore selects another directory instead of overwriting an
incompatible corpus. The hash shown above is illustrative.

Each file is a complete, versioned snapshot for the configured endpoint pairs at
that routing timestamp. For one pair and five paths it contains only five node
sequences plus metadata, so full companion snapshots are small enough that a
second delta layer is unnecessary.

Format version 2 uses little-endian 32-bit words. Its fixed header is followed by
one canonical unordered endpoint-pair record per configured pair and then each
path's delay fields and variable-length stable node-ID sequence. Delay values are
stored using their IEEE 754 binary64 bit representation.

Version 1 companion files used the removed candidate-pool policy and are not
loaded by this implementation. Regenerate those small companion files; primary
routing snapshots are unaffected.

Different endpoint geometries require different set names. For example, San
Diego-to-Seattle and London-to-Shanghai cannot share an endpoint path set even
when they use the same primary ISL routing corpus. All protocols and repeated
runs using one geometry can reuse its generated set.

## Validation

Every file is atomically published and records:

- Version, sequence, and numeric timestamp.
- Routable and total node counts.
- K count, solver identity, RTT range, and edge-disjoint policy.
- The primary route-state hash for the same timestamp.
- Endpoint attachment and access-delay state.
- Canonical endpoint pairs, attached core nodes, path delays, and complete node
  sequences.
- A payload checksum and exact path/node counts.

A missing, truncated, malformed, out-of-order, wrong-policy, wrong-route, or
wrong-endpoint file fails before the live catalog changes. A terminal that is
disconnected during hard-handover downtime has a valid group with zero paths.
Changing an endpoint attachment clears the previous catalog until the matching
routing-timestamp snapshot is loaded.

## Endpoint Semantics

User terminals map to their currently attached satellite before the core query.
Their access links are included in every propagation RTT:

```text
RTT = 2 * (source access delay + core path delay + destination access delay)
```

Access links are excluded from edge-disjointness, so every path may share a user
terminal's only satellite link and then diverge in the LEO core. Ground-station
links are part of the core and can provide immediate path diversity.

These are propagation RTT estimates. They do not include serialization,
queueing, or transport processing time.

For ordinary K-shortest queries, the installed igraph
`igraph_get_k_shortest_paths()` implementation uses Yen's algorithm and returns
loopless weighted paths in increasing length order.

Edge-disjoint queries use a separate exact min-cost-flow solver. Every undirected
physical core link has shared unit capacity, so it cannot be used in either
direction by more than one selected path. Successive shortest augmentations find
the maximum available path count up to K and minimize total one-way propagation
delay for that count. This avoids the old finite candidate pool and greedy choice
that could miss an existing disjoint set.

The selected paths are sorted by RTT and paths outside `kPathMaxRttSpread` of
the fastest selected path are removed. A group can therefore contain fewer paths
than requested because of topology degree or the RTT range. The min-cost-flow
problem itself is exact; the subsequent RTT filter is deliberately not a
length-constrained disjoint-path optimization.

## Query API

The loaded catalog can be queried in either direction by stable endpoint ID:

```cpp
leoRouting::KShortestPathGroup group =
    configurator->getKShortestPathGroup(sourceNodeId, destinationNodeId, 4);
```

or by mapped IPv4 address:

```cpp
leoRouting::KShortestPathGroup group =
    configurator->getKShortestPathGroupForAddresses(sourceAddress, destinationAddress, 4);
```

Reverse-direction queries reverse each saved node sequence without storing the
same symmetric path twice. Requesting fewer than the saved maximum truncates the
ordered group without recomputation. This compact representation relies on the
current LEO model's bidirectional, symmetric-delay links; a future directed-link
model must store directions separately.

The query returns a value containing the node sequences, so a transport should
fetch it once per catalog generation and retain it for its subflows rather than
calling the API for every packet.

Future transport integration still needs a packet path identifier, source
routing, segment routing, or per-flow forwarding entries. A first-hop choice
alone cannot guarantee that downstream routers follow the selected complete
path.
