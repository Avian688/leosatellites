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

- OMNeT++ 6.x, installed and built with its platform dependencies
- **[Avian688/inet4.5](https://github.com/Avian688/inet4.5), imported as the `inet4.5` project. This fork is required; do not substitute the upstream INET repository or an IDE-downloaded INET release.**
- [OS3](https://github.com/Avian688/os3)
- [igraph](https://igraph.org/c/) >= 0.10, including its development headers
- libcurl development headers and libraries
- `pkg-config`, Python 3.9 or newer, and Git
- A C++17-capable compiler and GNU Make 4.x
- Optional: OpenSceneGraph and an OMNeT++ build with `WITH_OSG=yes` for the
  Qtenv 3D viewer. The plain OSG viewer does not require osgEarth.

First install OMNeT++ using the [installation guide](https://docs.omnetpp.org/installguide/index)
for your platform and chosen OMNeT++ version, including its compiler, Qt, and
Python dependencies. The commands below install the additional LEO dependencies
and basic build tools; they do not replace the OMNeT++ installation procedure.

On **macOS**, install Xcode Command Line Tools if needed (`xcode-select --install`),
then use [Homebrew](https://brew.sh/):

```bash
brew install git python pkgconf make igraph curl
```

Homebrew's [pkgconf](https://formulae.brew.sh/formula/pkgconf) supplies
`pkg-config`; its [GNU Make](https://formulae.brew.sh/formula/make) is also
available as `gmake`. Use GNU Make 4.x rather than macOS's bundled Make 3.81;
the OMNeT++ environment normally adds Homebrew's GNU Make to `PATH`.

On **Ubuntu 24.04**, install:

```bash
sudo apt update
sudo apt install build-essential git python3 pkg-config libigraph-dev libcurl4-openssl-dev
```

Other distributions need equivalent development packages. Check the installed
igraph version: older distributions may ship a version below 0.10. Installing
only the `curl` command-line program is not sufficient; the development package
is needed. For the optional 3D viewer, follow the OpenSceneGraph instructions in
the OMNeT++ installation guide before configuring/building OMNeT++.

The expected workspace layout is:

```text
<omnetpp-root>/samples/
|-- inet4.5/
|-- os3/
`-- leosatellites/
```

For a fresh workspace, clone these repositories into that layout (skip any
that are already present):

```bash
cd /path/to/omnetpp/samples
git clone https://github.com/Avian688/inet4.5.git inet4.5
git clone https://github.com/Avian688/os3.git os3
git clone https://github.com/Avian688/leosatellites.git leosatellites
```

Keep the `inet4.5` and `os3` directory/project names: the IDE project references
use those names. Import all three projects using **File > Import > General >
Existing Projects into Workspace**. Generate their Makefiles as required and
build **Avian688/inet4.5 first, then OS3**, following their own build instructions.
Use the same build mode (release or debug) for all three projects.

## Building

After installing the dependencies above, load the OMNeT++ environment in each
new terminal and configure LEO:

```bash
cd /path/to/omnetpp
source setenv
cd samples/leosatellites
python3 setup.py --check
python3 setup.py
```

This detects the installed igraph and curl flags through `pkg-config`, finds
sibling `inet4.5` and `os3` projects, and runs `opp_makemake` with the shared-library
settings. On macOS it also discovers Homebrew's package metadata, including
keg-only curl. It does not install packages, generate INET/OS3 Makefiles, or
compile any project. Build INET and OS3 separately before compiling LEO.

Machine-specific settings are written to Git-ignored `config.local.mk`.
`src/makefrag` reads the current package flags for both CLI and IDE builds;
versioned Homebrew paths and dependency libraries no longer need manual editing.
After moving the checkout or changing dependency locations, run setup again.

Use these options for a different directory layout or a read-only check:

```bash
python3 setup.py --inet /path/to/inet --os3 /path/to/os3
python3 setup.py --check
```

`INET_ROOT` and `OS3_ROOT` provide defaults for those options. For dependencies
in custom prefixes, set `PKG_CONFIG_PATH` before setup; explicit search paths
are preferred over discovered Homebrew paths. `make setup` and `make makefiles`
also run setup. Run this once after cloning; build INET and OS3 before LEO:

```bash
make MODE=release
# Or: make MODE=debug
```

For the IDE, import all three projects, keep `inet4.5` and `os3` checked in
**Project Properties > Project References**, and refresh leosatellites after
setup. The portable Makemake metadata uses the same `src/makefrag`, so IDE
Makefile regeneration retains automatic dependency discovery. If the IDE does
not inherit the terminal's OMNeT++ environment, configure its normal OMNeT++
installation settings. CLI builds also require that environment.

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

Choose the queue independently of the interface, including on interfaces created
later during a handover:

```ini
**.ppp[*].queue.typename = "inet.queueing.queue.DropTailQueue"
```

Use fully qualified NED names, such as `inet.queueing.queue.DropTailQueue`
or `inet.queueing.queue.DropHeadQueue`. Third-party queues implementing
`IPacketQueue` can also be selected when their NED path and library are loaded
by the consuming application. No change to leosatellites is required.

`queueSize` supplies a default packet capacity through the interface's
`defaultPacketCapacity` construction hook. An explicit queue setting wins:

```ini
*.satellite[*].ppp[*].queue.packetCapacity = 500
```

This is configured before queue initialization, so the queue does not need a
mutable `packetCapacity`. Queues without that parameter are left alone. Compound
queues may need their inner queues/buffer configured separately; LEO does not
invent capacity or scheduling semantics for them. A custom PPP interface can
opt into the same default by declaring `defaultPacketCapacity` and assigning
`queue.packetCapacity = default(defaultPacketCapacity)` as a parameter pattern.
Otherwise its own queue configuration is used without constructor overrides.

The queue is a sibling of the `ppp` protocol module: use `ppp[*].queue`,
not `ppp[*].ppp.queue`.

The compatibility contract is INET's `IPacketQueue` (a passive packet input and
output compatible with PPP). Required queue-specific parameters still need to
be configured. Protocol-specific telemetry support remains the responsibility
of the selected queue and transport implementation.

## Saved routing snapshots

For transport-independent primary/K-path generation and the 3D viewer, use the
[shared routing examples](simulations/examples/routing/README.md). Both modes
share one terminal per ground station, all terminal pairs, and the
`simulations/leoSaves` store.


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

### The project cannot find igraph, curl, INET, or OS3

Run `python3 setup.py --check` from an OMNeT++ environment. If package metadata
is missing, install the corresponding development package or set
`PKG_CONFIG_PATH`. Use `--inet` and `--os3` for nonstandard project locations,
then rerun `python3 setup.py` with those options. Do not copy another machine's
`config.local.mk`.

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
