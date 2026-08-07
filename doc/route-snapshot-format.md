# LEO Routing Snapshot Format

LEO3 is the delta-encoded routing format used by `LeoIpv4NetworkConfigurator`.
It keeps the existing constellation-directory and timestamp-filename convention;
the file header identifies the format. All integers are signed 32-bit little-endian
words unless described as a two-word unsigned 64-bit value (low word first).

## LEO2 compatibility

LEO2 files begin with magic `0x4c454f32`, version `2`, followed by repeated
`(source, destination, stableNextHopNode)` triples. They are full snapshots.
Duplicate `(source,destination)` keys are canonicalised in file order, so the last
record wins. Loading LEO2 remains supported but necessarily rebuilds every source
row on every update.

## LEO3 header

LEO3 files begin with magic `0x4c454f33`, version `3`, and this 24-word header:

| Word(s) | Field |
| --- | --- |
| 0 | Magic (`LEO3`) |
| 1 | Version (`3`) |
| 2 | Header length (`24` words) |
| 3 | Kind (`1` base, `2` delta) |
| 4 | Sequence number |
| 5 | Base sequence number |
| 6 | Required preceding sequence (`-1` for base) |
| 7 | Routable source-node count |
| 8 | Stable destination/next-hop-node count |
| 9 | Payload record count |
| 10-11 | Timestamp in microseconds |
| 12-13 | Preceding effective-route count |
| 14-15 | Result effective-route count |
| 16-17 | Preceding state hash |
| 18-19 | Result state hash |
| 20-21 | Base-state hash |
| 22-23 | Ordered payload checksum |

The first file in numeric timestamp order is a complete base with sequence 0.
Every later file is a delta whose preceding sequence, route count, state hash, and
base hash must match the state already loaded. The timestamp in the header must
match the filename. These checks reject missing bases, skipped or reordered files,
truncation, payload corruption, and deltas from another corpus before forwarding
state is changed.

## Payload

Every payload record is `(source, destination, stableNextHopNode)`.

- In a base, every record is a SET and keys must be unique.
- In a delta, a non-negative next hop is an ADD or SET.
- In a delta, next hop `-1` is DELETE.
- Other negative next-hop values are invalid.
- Delta keys must be unique and unchanged SETs are rejected.

The state hashes are order-independent hashes of the effective
`(source,destination,nextHop)` map. The payload checksum is order-sensitive.
Files store stable node IDs, never runtime interface IDs.

## Interface changes

The configurator tracks interface-map changes per source. A normal delta resolves
only changed routes. If a source's mapping from stable next-hop node to interface ID
changed, its complete destination row is resolved again from the retained stable
route state. Every affected route is validated before any live forwarding row is
committed.

## Corpus selection

The leaf directory and filenames are unchanged, for example:

```text
ROUTING_ROOT/1584_550_72_22_53_100_ISL/286.000001.bin
```

Select a corpus root with the existing OMNeT++ parameter:

```ini
*.configurator.configLocation = "/path/to/ROUTING_ROOT"
```

The Experiment 8 generator also accepts this root through
`LEO_ROUTING_CORPUS_ROOT`. An empty value preserves the current working-directory
behavior.

## Conversion

Build and convert without changing the original corpus:

```sh
make -C samples/leosatellites/tools/route_snapshots
samples/leosatellites/tools/route_snapshots/leo_route_snapshot_tool \
  convert ORIGINAL_CONSTELLATION_DIRECTORY OUTPUT_ROOT --node-count 1684
```

The converter creates `OUTPUT_ROOT/<same-constellation-name>/`, writes each file
through a temporary path and atomic rename, and leaves an incomplete marker until
the whole sequence is complete. Verify every reconstructed state with:

```sh
samples/leosatellites/tools/route_snapshots/leo_route_snapshot_tool \
  verify ORIGINAL_CONSTELLATION_DIRECTORY \
  OUTPUT_ROOT/1584_550_72_22_53_100_ISL --node-count 1684
```
