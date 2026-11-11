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
`replayStart` defaults to `0s`. NoradA reads this parameter from the network's
`channelConstructor`.
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
The file records active ground/terminal links, installed channel delays, serving
satellites, pending handover targets and their original start/reconnect times,
the next attachment check, and the next topology update. It also records a
configuration signature and simulation time scale.
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

Each scheduled terminal check disconnects the serving link, including when
satellite coverage overlaps or the same satellite is selected again. The
reconnect event installs the selected link and restores its routes after the
modeled handover delay. Coverage loss between scheduled checks uses the same
timer. Reconnect deadlines are rounded up to the snapshot format's microsecond
precision. Exact endpoint catalogs store off-tick reconnect states under
`reconnects/` in the same policy directory. These snapshots use the previous primary sequence and
state hash, with the new attachment and access-delay hash. Only groups whose
endpoints changed are refreshed in a continuous run. At replay startup, a
checkpoint taken after such a reconnect loads the matching reconnect catalog
on top of the restored primary state.

The checkpoint's next update remains `170.000001s`, scheduled at local
`0.000001s`. Subsequent lookups add the offset to local time. The loader checks
the policy, exact timestamp, primary state, and endpoint attachments and access
delays for both kinds of K-path snapshot.

## Regenerate after the handover policy change

Rebuild LeoSatellites, then regenerate the exact endpoint catalog. For the
shared EdgeDisjoint catalog, run from `simulations/pythonScripts/routingExamples`:

```sh
python3 saveRoutes.py --mode multiple --start-step 2 --policies EdgeDisjoint
```

This reuses the primary core saves. They must cover the full generation duration
(300 s by default) and match the core topology and tick phase. Capture a fresh
checkpoint using the rebuilt library. Checkpoints from the previous handover
policy fail the configuration signature check.

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
history. Checkpoint tests cover exact serialization, a pending hard handover
across the boundary, attachment-check phase, and incompatible state.
In an ExperimentLeoLoadBalance replay, check that Bristol loses its routes
during the hard handover around local 65 s and gets them back in the reconnect
event. Unrelated pairs should retain their routes through both events.
