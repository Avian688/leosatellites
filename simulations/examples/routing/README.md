# Shared LEO route generation and viewing

These examples prepare routing data once, independently of any transport
experiment. They need INET, OS3, and leosatellites. Run the commands below from this directory.

The common scenario has 1,584 satellites, 72 planes, 22 satellites per plane,
550 km altitude, 53-degree inclination, and 100 ms mobility updates. It uses
a shared ordered ground-station list (five city stations plus the supplied
gateways) and ten terminals, in these five pairs:

| Pair | Source terminal | Destination terminal |
| --- | --- | --- |
| San Diego–Seattle | 0 | 1 |
| Seattle–New York | 2 | 3 |
| San Diego–New York | 4 | 5 |
| New York–London | 6 | 7 |
| San Diego–Shanghai | 8 | 9 |

Both save modes and the viewer are generated from the same definitions in
`../../pythonScripts/routingExamples/generateRoutingExamples.py` and
`ground_stations.txt` alongside it. There are no traffic applications enabled
during saving. The duration defaults to 300 seconds; specify `--sim-time` for
longer experiments.

## Example 1: primary shortest paths

```bash
python3 ../../pythonScripts/routingExamples/saveRoutes.py --mode shortest
```

This saves the primary shortest-path routing tables.
For the shortest-path 3D display as well, use:

```bash
python3 ../../pythonScripts/routingExamples/saveRoutes.py --mode shortest --with-viewer
python3 ../../pythonScripts/routingExamples/viewRoutes.py --policy ShortestPath --pair SanDiegoToShanghai
```

The viewer reads K-path catalogs, so `--with-viewer` additionally generates the
Unrestricted ten-path catalog; ShortestPath displays only its first rank.
Primary routing files alone are sufficient for normal single-path experiments.

## Example 2: multiple paths

```bash
python3 ../../pythonScripts/routingExamples/saveRoutes.py --mode multiple --cores 7
python3 ../../pythonScripts/routingExamples/viewRoutes.py --policy EdgeDisjoint --pair SanDiegoToShanghai
```

Primary routes are generated first, then the seven catalogs in parallel:
Unrestricted, Shared1–Shared5, and EdgeDisjoint. Each catalog requests ten paths
with a maximum RTT spread of 1,000 ms; fewer may be available. To reuse existing
primary routes and generate selected catalogs only:

```bash
python3 ../../pythonScripts/routingExamples/saveRoutes.py --mode multiple --start-step 2 --policies Unrestricted EdgeDisjoint --cores 2
```

Add `--topology GroundRelay` to saving and viewing commands for ISLs disabled.
ISL and GroundRelay use separate `_ISL` and `_BP` directories. `--no-isls` on
the viewer hides lines only. Viewing requires an OSG-enabled build and Qtenv.

For IDE launches, first run `generateRoutingExamples.py`. Then use
`shortestPath.ini` / `SaveShortestPaths`, or `multiplePaths.ini` /
`SaveMultiplePaths` after primary generation. Both files also have GroundRelay
variants. Other policy configs are in generated `routing.ini`; viewer configs
are in generated `visualize.ini`.

## Reusing the corpus in another experiment

All saves live in `leosatellites/simulations/leoSaves`. Generation allows replacing snapshots on reruns. Generate a
duration covering the entire consumer run. Avoid simultaneous writers to the
same primary corpus or policy catalog.

Use the generated `topology.ini` as the shared configuration base. It contains
the common constellation, ordered station/terminal coordinates and routing
parameters, without the probe app assignments. For example, copy or include it
from a consumer INI, then add a config like:

```ini
include /absolute/path/to/leosatellites/simulations/examples/routing/topology.ini

[Config MyTransportExperiment]
# Defaults to LeoRoutingNetwork; extend that network for additional features.
*.configurator.configLocation = "/absolute/path/to/leosatellites/simulations/leoSaves"
**.loadFiles = true
*.configurator.kPathSnapshotMode = "disabled"
# Add transport, queue, and application parameters here.
```

Paths in INI parameter values are relative to the **launch working directory**,
not the included file. Set `configLocation` explicitly in downstream setups.
Use an experiment-owned result directory. Add leosatellites to the consuming
project's references and include its `src` and `simulations` NED roots at launch.

For multipath consumers set `kPathSnapshotMode = "load"`,
`kPathSnapshotSet = "SharedLeoRoutes"`, `numOfKPaths = 10`, and
`kPathMaxRttSpread = 1000ms`, with the desired `kPathsEdgeDisjoint` /
`kPathMaxSharedLinks` policy. Keep the endpoint-pair specification from the
shared include. Transport route selection still needs its normal configuration
(for example, the selected subflow/path ranks); loading a catalog does not enable
that automatically. A K=2 transport can use the first two ranks of the K=10
catalog without changing the catalog's `numOfKPaths`.

**Compatibility requires the same graph:** constellation geometry, node counts,
node ordering, station/terminal coordinates, ISL mode, mobility timing, and
handover settings. Preserve the `satellite`, `groundStation`, and `userTerminal`
module arrays. Experiments with different terminal counts must adopt the
shared layout before reusing these files, or generate a separate corpus.
Catalogs from different geometries or profiles must not be mixed by renaming
files into the `SharedLeoRoutes` snapshot set.

`routing-profile.json` records the generated configuration for comparison. It
is not a completion marker or an automatic compatibility validator.

Route saves, logs, results, generated INIs, the generated profile, and Python
caches are ignored by Git. The scripts, two example entry INIs, network models,
and station input remain tracked. No generated corpus is shipped.
