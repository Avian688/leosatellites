# Shared LEO route generation and viewing

These examples prepare routing data once, independently of any transport
experiment. They need INET, OS3, and leosatellites. Run the commands below from this directory.

The common scenario has 1,584 satellites, 72 planes, 22 satellites per plane,
550 km altitude, 53-degree inclination, and 100 ms mobility updates. It uses
a shared ordered ground-station list (five city stations plus the supplied
gateways). There is exactly one user terminal at each ground-station location,
with matching array indices. The supplied list gives **100 terminals and 4,950
unordered terminal pairs**. Every pair is included in each K-path policy catalog;
reverse-direction queries reuse the same saved group.

K-path endpoints are user terminals only. Ground stations remain possible core
relays. The first and last terminal-to-satellite access links may be shared across
all ranks, including edge-disjoint ranks; disjointness applies to core links.

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
The primary saver still records core forwarding routes, including ground stations;
only the K-path catalogs are restricted to user-terminal endpoint pairs.

## Example 2: multiple paths

```bash
python3 ../../pythonScripts/routingExamples/saveRoutes.py --mode multiple --cores 7
python3 ../../pythonScripts/routingExamples/viewRoutes.py --policy EdgeDisjoint --pair SanDiegoToShanghai
```

Primary core routes are generated first, then the seven terminal-pair catalogs in parallel:
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
`kPathSnapshotSet = "SharedLeoAllStationPairs"`, `numOfKPaths = 10`, and
`kPathMaxRttSpread = 1000ms`, with the desired `kPathsEdgeDisjoint` /
`kPathMaxSharedLinks` policy. Keep the endpoint-pair specification from the
shared include. Transport route selection still needs its normal configuration
(for example, the selected subflow/path ranks); loading a catalog does not enable
that automatically. A K=2 transport can use the first two ranks of the K=10
catalog without changing the catalog's `numOfKPaths`.

For later experiments with additional terminals, use `kPathSnapshotMode = "load"`
and set `kPathCoreSnapshotProfile` to the existing policy-profile directory name
under `kpaths/SharedLeoAllStationPairs/`. Configure the consumer's traffic pairs.
This reuses saved core ranks and appends the current first and last access links,
without recomputing K paths. The satellite/ground-station core, its geometry,
node ordering and mobility timing must match the primary saves.

Each consumer's serving-satellite pair must be covered by the saved catalog at
that time. This setup covers all station-location terminal pairs; it does not
promise every possible satellite pair for arbitrary other geographic locations.
Without `kPathCoreSnapshotProfile`, exact saved-catalog loading still requires
matching endpoint geometry and attachments. Additional terminals must be present
at simulation initialization; runtime module creation is not provided here.

`routing-profile.json` records the generated configuration for comparison. It
is not a completion marker or an automatic compatibility validator.

Route saves, logs, results, generated INIs, the generated profile, and Python
caches are ignored by Git. The scripts, two example entry INIs, network models,
and station input remain tracked. No generated corpus is shipped.

The all-station catalogs use `SharedLeoAllStationPairs`, separate from older
limited-pair catalogs. Regenerate K-path saves before using this setup; refreshing
the INI files alone does not produce route data. Generation covers many more
pairs, so use fewer `--cores` if concurrent policy generation exceeds memory.
