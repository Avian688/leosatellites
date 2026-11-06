# Saved-route replay windows

Replay is opt-in, controlled by **one** offset:

```ini
sim-time-limit = 100s
*.channelConstructor.replayStart = 170s
*.channelConstructor.replayStateFile = "/path/to/170.leoreplay"
**.loadFiles = true
*.configurator.kPathSnapshotMode = "load"
```

Use the same constellation, epoch, station/terminal ordering and locations,
attachment settings, and saved K-path policy/profile as the original run.
`replayStart` defaults to `0s`; existing configurations keep their original
behaviour. NoradA reads this parameter from the network's `channelConstructor`.
Its propagation uses `targetTime + replayStart`, including future-position
queries, without changing the orbit epoch. Event times and application clocks
remain local: local 0–100 s corresponds to original 170–270 s.

## Capture once, reuse across runs

A serving satellite cannot in general be recovered by rerunning selection at
170 s: selection history and pending hard handovers matter. Run the original
zero-offset topology once, loading the existing primary saves, with:

```ini
sim-time-limit = 170.000002s
**.loadFiles = true
*.configurator.kPathSnapshotMode = "disabled"
*.channelConstructor.captureReplayAt = 170s
*.channelConstructor.replayStateFile = "/path/to/170.leoreplay"
```

This captures the state immediately **before** events at original 170 s.
No route regeneration is needed. The file records active ground/terminal links,
installed channel delays, serving satellites, pending handover targets and their
original start/reconnect times, the next attachment check, and the next topology
update. It also records a configuration signature and simulation time scale.
A checkpoint can be reused across transport runs and K-path policies with the
same topology. Existing checkpoint files are not overwritten automatically.

On replay, live PPP interfaces and channels are recreated before primary
forwarding is installed. A pending reconnect at original 170.02 s is scheduled
at local 0.02 s. The next 15 s attachment check remains original 180 s (handled
by the topology update at about local 10.000001 s).

`simulations/examples/routing/replay.ini` provides `CaptureReplay170` and
`Replay170To270` with the shared ten-pair topology. Generate `topology.ini` using
`generateRoutingExamples.py` first, then launch these configurations from the
routing example directory. The replay example selects the existing EdgeDisjoint
catalog; change its policy parameters to select another saved catalog. It has
no application traffic; consumers add their own applications.

## Route timeline and validation

At local zero, the loader reconstructs the primary table from `0.bin` and all
subsequent base/delta history strictly before the boundary. For 170 s, this
ends at `169.900001.bin`. Historical routes are validated in memory; only the
final table is resolved onto current interfaces. Original sequence, base and
state hashes are preserved, so the next delta validates normally.

If attachments changed after the last primary snapshot, the original endpoint
catalog was cleared: replay preserves that absence until the next regular
snapshot. Otherwise, the matching initial K-path snapshot is loaded with the existing policy,
sequence, primary-state and endpoint attachment/access-delay checks intact.
The checkpoint's next update remains `170.000001s`, scheduled at local
`0.000001s`; subsequent lookups add the offset to local time. There is no
`170.bin` lookup and no regeneration of K paths. Missing, off-phase or corrupt
primary history is rejected. K-path attachment mismatches are still errors;
the offset does not make a catalog from different terminal history compatible.

The initial implementation requires positive boundaries aligned to the common
satellite mobility update interval (170 s is aligned to 100 ms), NoradA
satellites, and unchanged geometry/attachment parameters. Use the same
simulation time resolution for capture and replay. Capture and offset playback
are separate runs. Primary saves must include the prefix from zero and the
whole replay window; K-path saves must include the initial snapshot and the
window. `kPathSnapshotMode="generate"` is rejected for offset replay.

This is a topology and route replay, not a transport checkpoint: queues, packets,
TCP state and accumulated application statistics start fresh. Checkpoint files
ending in `.leoreplay` and their temporary files are ignored by Git.

## Validation

Standalone tests exercise primary reconstruction through 169.900001 s and the
next 170.000001 s delta, preserving sequence/hash metadata and rejecting missing
history. Checkpoint tests cover exact serialization, a pending handover across
the boundary, attachment-check phase, and malformed/incompatible state.
Full continuous-versus-offset OMNeT++ runs remain an integration validation step.
