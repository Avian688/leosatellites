# leosatellites: LEO constellation simulation for OMNeT++ and INET

`leosatellites` provides mobility, connectivity, routing, and endpoint models
for experimenting with Low Earth Orbit (LEO) satellite networks in OMNeT++ and
INET. It builds on the OS3 satellite framework and adds configurable synthetic
constellations, dynamic inter-satellite and access links, reusable routing
snapshots, user terminals, and explicit hard-handover behavior.

The primary example network is `SatSGP4`, located in
[`simulations/SatSGP4`](simulations/SatSGP4).

## Core features

- **Configurable constellations:** Set the satellite count, orbital planes,
  satellites per plane, altitude, inclination, and mobility update interval.
- **SGP4-based mobility:** `NoradA` creates synthetic constellations from
  orbital parameters without requiring a TLE file.
- **ISL and bent-pipe topologies:** Enable inter-satellite links for a routed
  mesh, or disable them for ground-relay and bent-pipe experiments.
- **Dynamic access links:** Ground-station and user-terminal links are created,
  updated, and removed according to satellite visibility.
- **Delay-aware routing:** The IPv4 configurator recomputes shortest paths as
  the constellation changes.
- **Saved routing snapshots:** Generate complete binary route tables once and
  replay them in later simulations to avoid repeating shortest-path work.
- **User terminals:** Place INET `StandardHost` terminals at arbitrary
  coordinates and run normal INET applications on them.
- **Hard handovers:** User-terminal satellite changes use break-before-make
  behavior with an explicit outage and queryable handover state.

## Requirements

The current project setup targets:

- OMNeT++ 6.x
- INET 4.5, imported as the `inet4.5` project
- [OS3](https://github.com/Avian688/os3)
- [igraph](https://igraph.org/) 0.10.x and its development headers
- A C++17-capable compiler

The expected workspace layout is:

```text
<omnetpp-root>/samples/
|-- inet4.5/
|-- os3/
`-- leosatellites/
```

The IDE project already references `inet4.5` and `os3`. Build those projects
before building `leosatellites`.

## Building

The recommended setup uses the OMNeT++ IDE:

1. Import `inet4.5`, `os3`, and `leosatellites` into the same workspace.
2. Open **Project Properties > Project References** and ensure `inet4.5` and
   `os3` are selected.
3. Open **Project Properties > OMNeT++ > Makemake**, edit the `src` target, and
   set the igraph include and library paths for your machine.
4. Regenerate the `src/Makefile` and build in this order: INET, OS3,
   `leosatellites`.

The committed Makemake metadata contains example local paths. These paths must
be updated when the project is cloned onto another machine. After generating a
valid `src/Makefile`, command-line builds can be run from the project root:

```bash
make MODE=release
make MODE=debug
```

Before using OMNeT++ commands in a terminal, load the OMNeT++ environment from
the installation root:

```bash
cd <omnetpp-root>
. setenv
```

## Quick start

The supplied `SatSGP4` example has separate configurations for generating and
loading routing snapshots:

1. Open [`simulations/SatSGP4/omnetpp_saveRouting.ini`](simulations/SatSGP4/omnetpp_saveRouting.ini).
2. Run `Experiment1SaveRouting` to generate route snapshots.
3. Open [`simulations/SatSGP4/omnetpp.ini`](simulations/SatSGP4/omnetpp.ini).
4. Run `Experiment1` to load those snapshots and execute the experiment.

The checked-in example has a short simulation limit. Increase
`sim-time-limit` in both configurations when generating routes for a longer
experiment.

## Configuring a constellation

The following example defines a 1,584-satellite constellation with 72 planes
and 22 satellites per plane:

```ini
[Config BaseConstellation]
network = SatSGP4
sim-time-limit = 300s

**.numOfSats = 1584
**.satsPerPlane = 22
**.numOfPlanes = 72
**.incl = 53
**.alt = 550

*.satellite[*].NoradModule.satIndex = parentIndex()
*.satellite[*].NoradModule.satName = "sat"
*.satellite[*].NoradModule.inclination = 53 * 0.017453292519943
*.satellite[*].NoradModule.altitude = 550

SatSGP4.satellite[*].mobility.updateInterval = 100ms
**.enableInterSatelliteLinks = true
```

`incl` is expressed in degrees and `alt` in kilometres. They identify the
constellation and its saved-route directory. `NoradModule.inclination` is in
radians and `NoradModule.altitude` is in kilometres; keep both sets of values
consistent.

The main constellation parameters are:

| Parameter | Purpose |
| --- | --- |
| `numOfSats` | Number of instantiated satellites. |
| `satsPerPlane` | Number of satellite slots in each orbital plane. |
| `numOfPlanes` | Number of orbital planes used for RAAN spacing. |
| `incl` | Inclination in degrees used to identify the constellation. |
| `alt` | Altitude in kilometres used to identify the constellation. |
| `satellite[*].NoradModule.inclination` | Orbital inclination in radians. |
| `satellite[*].NoradModule.altitude` | Orbital altitude in kilometres. |
| `satellite[*].mobility.updateInterval` | Mobility, link, and route update interval. |
| `enableInterSatelliteLinks` | Enables the ISL mesh when `true`; uses bent-pipe connectivity when `false`. |

### Ground stations

Ground stations use latitude and longitude in degrees:

```ini
**.numOfGS = 2

*.groundStation[0].cityName = "London"
*.groundStation[0].mobility.latitude = 51.5074
*.groundStation[0].mobility.longitude = -0.1278

*.groundStation[1].cityName = "New York"
*.groundStation[1].mobility.latitude = 40.7128
*.groundStation[1].mobility.longitude = -74.0060
```

The channel constructor controls the capacity and packet queue used by
dynamically created satellite links:

```ini
*.channelConstructor.dataRate = 100Mbps
*.channelConstructor.queueSize = 300
*.channelConstructor.interfaceType = "leosatellites.linklayer.ppp.PppInterfaceMutable"
```

`queueSize` is applied as the queue's packet capacity.

## Saved routing snapshots

Dynamic shortest-path calculation is expensive for a large constellation.
`leosatellites` therefore supports a two-stage workflow:

```ini
[Config GenerateRoutes]
extends = BaseConstellation
**.loadFiles = false
*.visualizer.typename = ""

[Config ReplayRoutes]
extends = BaseConstellation
**.loadFiles = true
```

### Generate routes

With `loadFiles = false`, `leosatellites` recomputes shortest paths at every
mobility update and writes a complete binary routing snapshot for that
simulation time. Generation is intentionally slower, but the resulting files
can be reused by multiple protocol experiments.

Route generation writes a directory in the simulation working directory using
this key:

```text
<satellites>_<altitude>_<planes>_<satellites-per-plane>_<inclination>_<ground-stations>_<ISL|BP>/
```

For the example above with two ground stations, the directory is:

```text
1584_550_72_22_53_2_ISL/
```

Regenerating routes replaces an existing directory with the same key. Preserve
route sets elsewhere if they are still needed. Do not run two generators for
the same route-set directory concurrently.

### Load routes

With `loadFiles = true`, `leosatellites` reads the full snapshot corresponding
to each mobility update instead of rerunning the shortest-path calculation. To
load a route directory stored outside the current simulation directory, set a
base location with a trailing path separator:

```ini
*.configurator.configLocation = "../savedRoutes/"
```

The loader will then look for, for example:

```text
../savedRoutes/1584_550_72_22_53_2_ISL/
```

Route replay does not use approximations or route deltas. It applies the saved
full snapshot at the same update time at which it was generated.

Route files are valid only when all route-relevant settings match. In
particular, keep the following identical between generation and replay:

- Simulation start time and mobility update interval
- Satellite count, planes, satellites per plane, altitude, and inclination
- ISL or bent-pipe mode
- Ground-station count and coordinates
- Elevation and reachability settings
- Routing metric and topology
- Generated time range must cover the complete replay duration

The directory name does not encode every one of these values. If any setting
changes, regenerate the route files even if the generated directory name would
be unchanged. Generate snapshots for at least the full duration of the later
experiment.

If the loader reports a legacy route format, regenerate the route set with the
current version. This is required when using saved routes with user terminals.

## User terminals

`UserTerminal` extends INET's `StandardHost`, so normal INET applications and
transport protocols can run directly on a terminal. Unlike a fixed ground
station, each terminal maintains one serving-satellite access link and can move
between visible satellites.

```ini
**.numOfUserTerminals = 2
**.enableHandoverOracle = true

*.channelConstructor.userTerminalUpdateInterval = 5s
*.channelConstructor.userTerminalSampleInterval = 1s
*.channelConstructor.userTerminalHandoverDowntime = 50ms

*.userTerminal[0].terminalName = "London terminal"
*.userTerminal[0].mobility.latitude = 51.5074
*.userTerminal[0].mobility.longitude = -0.1278
*.userTerminal[0].numApps = 1
*.userTerminal[0].app[0].typename = "PingApp"
*.userTerminal[0].app[0].destAddr = "userTerminal[1]"

*.userTerminal[1].terminalName = "New York terminal"
*.userTerminal[1].mobility.latitude = 40.7128
*.userTerminal[1].mobility.longitude = -74.0060
```

The terminal selector samples satellite visibility, elevation, and azimuth
over the next selection interval. It uses those trajectories to choose a
currently reachable serving satellite that follows the strongest available
path as closely as possible.

The user-terminal parameters are:

| Parameter | Purpose |
| --- | --- |
| `numOfUserTerminals` | Number of dynamic user-terminal hosts. |
| `userTerminalUpdateInterval` | Interval between planned serving-satellite selections. |
| `userTerminalSampleInterval` | Sampling resolution used while evaluating candidate trajectories. |
| `userTerminalHandoverDowntime` | Fallback outage when an RTT-derived outage cannot be calculated. |
| `enableHandoverOracle` | Exposes current and predicted handover state through `UserTerminalHandoverOracle`. |

## Hard handovers

User-terminal satellite changes are modelled as hard, break-before-make
handovers:

1. The old satellite link is disconnected.
2. The terminal remains disconnected for the handover outage.
3. The new link is created only if the target satellite is still reachable.
4. Interfaces and endpoint routes are refreshed after reconnection.

When both access paths are known, the outage is calculated from their
propagation RTTs:

```text
handover downtime = 1.5 * (old access RTT + new access RTT)
```

`userTerminalHandoverDowntime` is a fallback, not an override of the
RTT-derived value.

When `enableHandoverOracle = true`, the optional
`UserTerminalHandoverOracle` records, per terminal:

- Whether a hard handover is active
- The handover start and expected end times
- The previous and target satellite indexes
- The next predicted handover time
- The most recent handover completion time

Transport protocols can query this module when they need explicit awareness
of an access-link outage. The handover itself still occurs when the oracle is
disabled; the oracle only publishes its state.

## Project structure

| Path | Contents |
| --- | --- |
| `src/mobility/` | Satellite and ground-node mobility models. |
| `src/libnorad/` | Orbital propagation support used by `NoradA`. |
| `src/common/LeoChannelConstructor.*` | Dynamic links, user-terminal selection, and hard handovers. |
| `src/common/UserTerminalHandoverOracle.*` | Queryable user-terminal handover state. |
| `src/networklayer/configurator/ipv4/` | Dynamic route generation and saved-route loading. |
| `src/networklayer/ipv4/` | LEO-aware IPv4 forwarding. |
| `src/linklayer/ppp/` | Mutable PPP interfaces used by dynamic links. |
| `simulations/SatSGP4/` | Reference network and route generation/replay examples. |

## Troubleshooting

### Routing files cannot be found

Check the simulation working directory, `configLocation`, constellation key,
simulation duration, and mobility update interval. A route file must exist for
each replay update time.

### Interface ID not found or packets become unroutable

The route set is probably stale or was generated for a different topology.
Regenerate it using exactly the same constellation and ground-station settings.

### Route generation is slow

This is expected for large constellations because all-pairs route information
is recalculated at each update. Generate the route set once with Cmdenv and no
visualizer, then use `loadFiles = true` for protocol experiments.

### The project cannot find igraph, INET, or OS3

Regenerate the Makemake configuration for the current machine. Do not reuse
absolute include and library paths from another installation.

## Citation

If `leosatellites` is used in published work, please cite:

```bibtex
@inproceedings{omnetpp-leosatellites-model,
  author = {Valentine, Aiden and Parisis, George},
  title = {{Developing and experimenting with LEO satellite constellations in OMNeT++}},
  booktitle = {Proceedings of the 8th OMNeT++ Community Summit Conference},
  address = {Hamburg, Germany},
  year = {2021}
}
```

## License

See [`LICENSE`](LICENSE) for the project's licensing terms.
