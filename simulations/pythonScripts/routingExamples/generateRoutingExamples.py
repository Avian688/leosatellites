#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
from collections import OrderedDict
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
SIMULATIONS_DIR = SCRIPT_DIR.parents[1]
EXPERIMENT_DIR = SIMULATIONS_DIR / "examples" / "routing"
INI_FILE = EXPERIMENT_DIR / "routing.ini"
VISUALIZATION_INI_FILE = EXPERIMENT_DIR / "visualize.ini"
GROUND_STATIONS_FILE = SCRIPT_DIR / "ground_stations.txt"

PATH_COUNT = 10
PING_INTERVAL_SECONDS = 0.05
PING_START_SECONDS = 1.0
PING_DRAIN_SECONDS = 2.0
MAX_RTT_SPREAD_MS = 1000
K_PATH_SNAPSHOT_SET = "SharedLeoRoutes"
ROUTE_STORE = "../../leoSaves"
OSG_EARTH_TEXTURE = "../../../../osg-satellites/earth.jpg"
TOPOLOGIES = ("ISL", "GroundRelay")

CITY_COORDINATES = {
    "San Diego": (32.7157, -117.1611),
    "Seattle": (47.6062, -122.3321),
    "New York": (40.7128, -74.0060),
    "London": (51.5074, -0.1278),
    "Shanghai": (31.2304, 121.4737),
}

# Separate terminals prevent one pair's selected route from affecting another.
# Their graph IDs are consecutive, so this order is also the configurator's
# canonical sorted endpoint-pair order used by KShortestPathPingApp.pathGroup.
PAIR_DEFINITIONS = (
    ("SanDiegoToSeattle", "San Diego", "Seattle", 0, 1),
    ("SeattleToNewYork", "Seattle", "New York", 2, 3),
    ("SanDiegoToNewYork", "San Diego", "New York", 4, 5),
    ("NewYorkToLondon", "New York", "London", 6, 7),
    ("SanDiegoToShanghai", "San Diego", "Shanghai", 8, 9),
)

# Ordered from least to most restrictive for plots and generated configs.
POLICIES = OrderedDict(
    (
        ("Unrestricted", {"label": "Unrestricted", "edge_disjoint": False, "max_shared": -1}),
        ("Shared5", {"label": "At most 5 shared links", "edge_disjoint": False, "max_shared": 5}),
        ("Shared4", {"label": "At most 4 shared links", "edge_disjoint": False, "max_shared": 4}),
        ("Shared3", {"label": "At most 3 shared links", "edge_disjoint": False, "max_shared": 3}),
        ("Shared2", {"label": "At most 2 shared links", "edge_disjoint": False, "max_shared": 2}),
        ("Shared1", {"label": "At most 1 shared link", "edge_disjoint": False, "max_shared": 1}),
        ("EdgeDisjoint", {"label": "Edge-disjoint", "edge_disjoint": True, "max_shared": -1}),
    )
)


def write_line(handle, line=""):
    handle.write(line + "\n")


def topology_config_name(base_name, topology):
    if topology not in TOPOLOGIES:
        raise ValueError(f"Unknown topology: {topology}")
    return base_name if topology == "ISL" else f"{base_name}_GroundRelay"


def load_ground_stations():
    if not GROUND_STATIONS_FILE.is_file():
        raise FileNotFoundError(f"Missing experiment ground-station list: {GROUND_STATIONS_FILE}")
    with GROUND_STATIONS_FILE.open(newline="", encoding="utf-8") as csv_file:
        return list(csv.DictReader(csv_file))


def endpoint_pair_specification():
    return ",".join(
        f"userTerminal[{source_index}]->userTerminal[{destination_index}]"
        for _, _, _, source_index, destination_index in PAIR_DEFINITIONS
    )


def write_policy_parameters(handle, policy):
    write_line(handle, f"*.configurator.kPathsEdgeDisjoint = {'true' if policy['edge_disjoint'] else 'false'}")
    write_line(handle, f"*.configurator.kPathMaxSharedLinks = {policy['max_shared']}")


def quote_ini_string(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def generate_visualization_ini(extra_ground_stations, sim_time_seconds):
    if sim_time_seconds <= 0:
        raise ValueError("Visualization simulation time must be positive")
    EXPERIMENT_DIR.mkdir(parents=True, exist_ok=True)
    city_ground_stations = list(CITY_COORDINATES.items())
    num_ground_stations = len(city_ground_stations) + len(extra_ground_stations)
    terminal_count = 2 * len(PAIR_DEFINITIONS)
    routing_directory_prefix = f"{ROUTE_STORE}/1584_550_72_22_53_{num_ground_stations}"
    routing_directory = f"{routing_directory_prefix}_ISL"
    terminal_pairs = ",".join(
        f"{source_index}-{destination_index}"
        for _, _, _, source_index, destination_index in PAIR_DEFINITIONS
    )

    with VISUALIZATION_INI_FILE.open("w", encoding="utf-8") as f:
        write_line(f, "[General]")
        write_line(f, "network = leosatellites.simulations.examples.routing.LeoRoutingOsg")
        write_line(f, f"sim-time-limit = {sim_time_seconds:g}s")
        write_line(f, "record-eventlog = false")
        write_line(f, "**.scalar-recording = false")
        write_line(f, "**.vector-recording = false")
        write_line(f, "**.bin-recording = false")
        write_line(f, "qtenv-default-config = View_Unrestricted")
        write_line(f)

        write_line(f, "*.numOfSats = 1584")
        write_line(f, f"*.numOfGS = {num_ground_stations}")
        write_line(f, "*.numOfClients = 0")
        write_line(f, f"*.numOfUserTerminals = {terminal_count}")
        write_line(f, "*.satsPerPlane = 22")
        write_line(f, "*.numOfPlanes = 72")
        write_line(f, "*.incl = 53")
        write_line(f, "*.alt = 550")
        write_line(f)

        write_line(f, f'*.pathVisualizer.earthTexture = "{OSG_EARTH_TEXTURE}"')
        write_line(f, f'*.pathVisualizer.routingDirectory = "{routing_directory}"')
        write_line(f, f'*.pathVisualizer.snapshotSet = "{K_PATH_SNAPSHOT_SET}"')
        write_line(f, f'*.pathVisualizer.endpointPairs = "{terminal_pairs}"')
        write_line(f, f"*.pathVisualizer.pathCount = {PATH_COUNT}")
        write_line(f, f"*.pathVisualizer.maxRttSpread = {MAX_RTT_SPREAD_MS}ms")
        write_line(f, "*.pathVisualizer.pairIndex = 0")
        write_line(f, "*.pathVisualizer.updateInterval = 1s")
        write_line(f, "*.pathVisualizer.animationSpeed = 1")
        write_line(f, "*.pathVisualizer.showInterSatelliteLinks = true")
        write_line(f)

        for ground_station_index, (city, coordinates) in enumerate(city_ground_stations):
            latitude, longitude = coordinates
            write_line(f, f'*.groundStation[{ground_station_index}].label = "{quote_ini_string(city)}"')
            write_line(f, f"*.groundStation[{ground_station_index}].latitude = {latitude}")
            write_line(f, f"*.groundStation[{ground_station_index}].longitude = {longitude}")

        ground_station_offset = len(city_ground_stations)
        for offset, entry in enumerate(extra_ground_stations):
            ground_station_index = ground_station_offset + offset
            label = quote_ini_string(entry["Location Comment"])
            write_line(f, f'*.groundStation[{ground_station_index}].label = "{label}"')
            write_line(f, f"*.groundStation[{ground_station_index}].latitude = {entry['Latitude']}")
            write_line(f, f"*.groundStation[{ground_station_index}].longitude = {entry['Longitude']}")
        write_line(f)

        for pair_key, source_city, destination_city, source_index, destination_index in PAIR_DEFINITIONS:
            source_latitude, source_longitude = CITY_COORDINATES[source_city]
            destination_latitude, destination_longitude = CITY_COORDINATES[destination_city]
            write_line(f, f"# {pair_key}")
            write_line(f, f'*.userTerminal[{source_index}].label = "{quote_ini_string(source_city)}"')
            write_line(f, f"*.userTerminal[{source_index}].latitude = {source_latitude}")
            write_line(f, f"*.userTerminal[{source_index}].longitude = {source_longitude}")
            write_line(f, f'*.userTerminal[{destination_index}].label = "{quote_ini_string(destination_city)}"')
            write_line(f, f"*.userTerminal[{destination_index}].latitude = {destination_latitude}")
            write_line(f, f"*.userTerminal[{destination_index}].longitude = {destination_longitude}")
        write_line(f)

        for policy_name, policy in POLICIES.items():
            write_line(f, f"[Config View_{policy_name}]")
            write_line(f, "extends = General")
            write_line(f, f'*.pathVisualizer.policyLabel = "{quote_ini_string(policy["label"])}"')
            write_line(f, f"*.pathVisualizer.edgeDisjoint = {'true' if policy['edge_disjoint'] else 'false'}")
            write_line(f, f"*.pathVisualizer.maxSharedLinks = {policy['max_shared']}")
            write_line(f, f'description = "3D view of {quote_ini_string(policy["label"])} paths"')
            write_line(f)

        write_line(f, "[Config View_ShortestPath]")
        write_line(f, "extends = View_Unrestricted")
        write_line(f, '*.pathVisualizer.policyLabel = "Shortest path"')
        write_line(f, "*.pathVisualizer.shortestPathOnly = true")
        write_line(f, 'description = "3D view of only the lowest-delay unrestricted path"')
        write_line(f)

        # Reuse each policy's display settings, but load the independently saved
        # bent-pipe topology. Hiding the ISL overlay alone does not change routes.
        for policy_name in ("ShortestPath", *POLICIES):
            base_config = f"View_{policy_name}"
            label = "Shortest path" if policy_name == "ShortestPath" else POLICIES[policy_name]["label"]
            write_line(f, f"[Config {topology_config_name(base_config, 'GroundRelay')}]")
            write_line(f, f"extends = {base_config}")
            write_line(f, f'*.pathVisualizer.routingDirectory = "{routing_directory_prefix}_BP"')
            write_line(f, "*.pathVisualizer.showInterSatelliteLinks = false")
            write_line(f, f'*.pathVisualizer.policyLabel = "Ground relay | {quote_ini_string(label)}"')
            write_line(f, f'description = "Ground-relayed 3D view: {quote_ini_string(label)} (ISLs disabled)"')
            write_line(f)

    print(f"Generated {VISUALIZATION_INI_FILE}")


def generate_ini(sim_time_seconds=300):
    if sim_time_seconds <= PING_START_SECONDS + PING_DRAIN_SECONDS:
        raise ValueError("Simulation time must leave room for the ping start and reply drain")
    extra_ground_stations = load_ground_stations()
    city_ground_stations = list(CITY_COORDINATES.items())
    num_ground_stations = len(city_ground_stations) + len(extra_ground_stations)
    terminal_count = 2 * len(PAIR_DEFINITIONS)

    EXPERIMENT_DIR.mkdir(parents=True, exist_ok=True)
    with INI_FILE.open("w", encoding="utf-8") as f:
        write_line(f, "[General]")
        write_line(f, "network = leosatellites.simulations.examples.routing.LeoRoutingNetwork")
        write_line(f, f"sim-time-limit = {sim_time_seconds:g}s")
        write_line(f, "result-dir = results")
        write_line(f, "record-eventlog = false")
        write_line(f, "cmdenv-express-mode = true")
        write_line(f, "cmdenv-event-banners = false")
        write_line(f, "**.cmdenv-log-level = off")
        write_line(f)

        # OMNeT++ uses the first matching configuration entry. Keep the
        # experiment's narrow enables above the broad recording disables.
        write_line(f, "**.rtt:vector.vector-recording = true")
        write_line(f, "**.pingTxSeq:vector.vector-recording = true")
        write_line(f, "**.kPathAvailable:vector.vector-recording = true")
        write_line(f, "**.kPathExpectedRtt:vector.vector-recording = true")
        write_line(f, "**.kPathCoreLinkCount:vector.vector-recording = true")
        write_line(f, "**.kPathCatalogSize:vector.vector-recording = true")
        write_line(f, "**.scalar-recording = false")
        write_line(f, "**.vector-recording = false")
        write_line(f, "**.bin-recording = false")
        write_line(f)

        write_line(f, "**.constraintAreaMinX = 0m")
        write_line(f, "**.constraintAreaMaxX = 2160m")
        write_line(f, "**.constraintAreaMinY = 0m")
        write_line(f, "**.constraintAreaMaxY = 1080m")
        write_line(f, "**.constraintAreaMinZ = 0m")
        write_line(f, "**.constraintAreaMaxZ = 0m")
        write_line(f)

        write_line(f, '*.*.ipv4.typename = "LeoIpv4NetworkLayer"')
        write_line(f, '**.ipv4.configurator.typename = "LeoIpv4NodeConfigurator"')
        write_line(f, '*.*.ipv4.arp.typename = "GlobalArp"')
        write_line(f, '*.*.ipv4.routingTable.netmaskRoutes = ""')
        write_line(f, '*.*.forwarding = true')
        write_line(f, '**.groundStation[*].mobility.typename = "GroundStationMobility"')
        write_line(f, '*.groundStation[*].mobility.initFromDisplayString = false')
        write_line(f, '*.groundStation[*].mobility.updateFromDisplayString = false')
        write_line(f, '**.userTerminal[*].mobility.typename = "GroundStationMobility"')
        write_line(f, '*.userTerminal[*].mobility.initFromDisplayString = false')
        write_line(f, '*.userTerminal[*].mobility.updateFromDisplayString = false')
        write_line(f)

        write_line(
            f,
            "*.configurator.config = xml(\"<config><interface hosts='**' address='10.x.x.x' "
            "netmask='255.x.x.x'/><autoroute metric='delay'/></config>\")",
        )
        write_line(f, "*.configurator.addStaticRoutes = true")
        write_line(f, "*.configurator.optimizeRoutes = false")
        write_line(f, f'*.configurator.configLocation = "{ROUTE_STORE}"')
        write_line(f, f"*.configurator.numOfKPaths = {PATH_COUNT}")
        write_line(f, f"*.configurator.kPathMaxRttSpread = {MAX_RTT_SPREAD_MS}ms")
        write_line(f, f'*.configurator.kPathSnapshotSet = "{K_PATH_SNAPSHOT_SET}"')
        write_line(f, f'*.configurator.kPathEndpointPairs = "{endpoint_pair_specification()}"')
        write_line(f, '*.configurator.kPathSnapshotMode = "disabled"')
        write_line(f, "*.configurator.kPathPingRouting = false")
        write_line(f, "*.configurator.allowRouteSnapshotOverwrite = false")
        write_line(f)

        write_line(f, '**.ppp[*].queue.typename = "DropTailQueue"')
        write_line(f, "**.ppp[*].queue.packetCapacity = 300")
        write_line(f, "**.satellite[*].NoradModule.satIndex = parentIndex()")
        write_line(f, '**.satellite[*].NoradModule.satName = "Starlink Satellite"')
        write_line(f, "**.satellite[*].**.bitrate = 100Mbps")
        write_line(f, '**.satellite[*].mobility.typename = "SatelliteMobility"')
        write_line(f, "**.satellite[*].mobility.updateInterval = 100ms")
        write_line(f)

        write_line(f, "**.numOfSats = 1584")
        write_line(f, "**.satsPerPlane = 22")
        write_line(f, "**.numOfPlanes = 72")
        write_line(f, "**.incl = 53")
        write_line(f, "**.satellite[*].NoradModule.inclination = 53*0.0174533")
        write_line(f, "**.alt = 550")
        write_line(f, "**.satellite[*].NoradModule.altitude = 550")
        write_line(f, f"**.numOfGS = {num_ground_stations}")
        write_line(f, f"**.numOfUserTerminals = {terminal_count}")
        write_line(f, "**.numOfClients = 0")
        write_line(f, "**.enableInterSatelliteLinks = true")
        write_line(f, "**.dataRate = 100Mbps")
        write_line(f, "**.queueSize = 300")
        write_line(f, "**.loadFiles = true")
        write_line(f, "**.userTerminalUpdateInterval = 15s")
        write_line(f)

        for ground_station_index, (city, coordinates) in enumerate(city_ground_stations):
            latitude, longitude = coordinates
            write_line(f, f"# {city} Ground Station")
            write_line(f, f'**.groundStation[{ground_station_index}].cityName = "{city}"')
            write_line(f, f"**.groundStation[{ground_station_index}].mobility.latitude = {latitude}")
            write_line(f, f"**.groundStation[{ground_station_index}].mobility.longitude = {longitude}")
            write_line(f)

        ground_station_offset = len(city_ground_stations)
        for offset, entry in enumerate(extra_ground_stations):
            ground_station_index = ground_station_offset + offset
            write_line(f, f"# {entry['Location Comment']} Ground Station")
            write_line(f, f'**.groundStation[{ground_station_index}].cityName = "{entry["Location Comment"]}"')
            write_line(f, f"**.groundStation[{ground_station_index}].mobility.latitude = {entry['Latitude']}")
            write_line(f, f"**.groundStation[{ground_station_index}].mobility.longitude = {entry['Longitude']}")
            write_line(f)

        for pair_group, (pair_key, source_city, destination_city, source_index, destination_index) in enumerate(PAIR_DEFINITIONS):
            source_latitude, source_longitude = CITY_COORDINATES[source_city]
            destination_latitude, destination_longitude = CITY_COORDINATES[destination_city]
            write_line(f, f"# {pair_key}")
            write_line(f, f'*.userTerminal[{source_index}].terminalName = "{source_city} source"')
            write_line(f, f"*.userTerminal[{source_index}].mobility.latitude = {source_latitude}")
            write_line(f, f"*.userTerminal[{source_index}].mobility.longitude = {source_longitude}")
            write_line(f, f'*.userTerminal[{destination_index}].terminalName = "{destination_city} destination"')
            write_line(f, f"*.userTerminal[{destination_index}].mobility.latitude = {destination_latitude}")
            write_line(f, f"*.userTerminal[{destination_index}].mobility.longitude = {destination_longitude}")
            for path_index in range(1, PATH_COUNT + 1):
                app_index = path_index - 1
                start_time = PING_START_SECONDS + pair_group * 0.002 + app_index * 0.01
                app = f"*.userTerminal[{source_index}].app[{app_index}]"
                write_line(
                    f,
                    f'{app}.typename = "leosatellites.applications.pingapp.KShortestPathPingApp"',
                )
                write_line(f, f'{app}.destAddr = "userTerminal[{destination_index}]"')
                write_line(f, f"{app}.pathGroup = {pair_group}")
                write_line(f, f"{app}.pathIndex = {path_index}")
                write_line(f, f"{app}.startTime = {start_time:.3f}s")
                write_line(f, f"{app}.stopTime = {sim_time_seconds - PING_DRAIN_SECONDS:g}s")
                write_line(f, f"{app}.sendInterval = {PING_INTERVAL_SECONDS:g}s")
                write_line(f, f"{app}.packetSize = 56B")
                write_line(f, f"{app}.count = -1")
                write_line(f, f"{app}.printPing = false")
            write_line(f)

        write_line(f, "[Config GenerateShortestPaths]")
        write_line(f, "extends = General")
        write_line(f, "**.loadFiles = false")
        write_line(f, '*.configurator.kPathSnapshotMode = "disabled"')
        write_line(f, "*.configurator.kPathPingRouting = false")
        write_line(f, "*.configurator.allowRouteSnapshotOverwrite = true")
        write_line(f, 'description = "Generate the primary shortest-path routing corpus"')
        write_line(f)

        for policy_name, policy in POLICIES.items():
            write_line(f, f"[Config Generate_{policy_name}]")
            write_line(f, "extends = General")
            write_line(f, '*.configurator.kPathSnapshotMode = "generate"')
            write_line(f, "*.configurator.kPathPingRouting = false")
            write_line(f, "*.configurator.allowRouteSnapshotOverwrite = true")
            write_policy_parameters(f, policy)
            write_line(f, f'description = "Generate {policy["label"]} K-path snapshots"')
            write_line(f)

            write_line(f, f"[Config Ping_{policy_name}]")
            write_line(f, "extends = General")
            write_line(f, '*.configurator.kPathSnapshotMode = "load"')
            write_line(f, "*.configurator.kPathPingRouting = true")
            for _, _, _, source_index, _ in PAIR_DEFINITIONS:
                write_line(f, f"*.userTerminal[{source_index}].numApps = {PATH_COUNT}")
            write_policy_parameters(f, policy)
            write_line(f, f'description = "Probe {policy["label"]} paths 1-{PATH_COUNT}"')
            write_line(f)

        # Keep the existing ISL configurations and corpora intact. The routing
        # configurator selects its separate _BP store when ISLs are disabled.
        for base_config in ("GenerateShortestPaths", *(f"Generate_{policy}" for policy in POLICIES)):
            write_line(f, f"[Config {topology_config_name(base_config, 'GroundRelay')}]")
            write_line(f, f"extends = {base_config}")
            write_line(f, "**.enableInterSatelliteLinks = false")
            write_line(f, f'description = "Ground-relayed routing: {base_config} (ISLs disabled)"')
            write_line(f)

    # Consumer experiments can include this without inheriting probe applications
    # or generation configs. They must set configLocation for their own cwd.
    general = INI_FILE.read_text().split("[Config GenerateShortestPaths]", 1)[0]
    topology = "\n".join(line for line in general.splitlines()
                         if ".app[" not in line) + "\n"
    (EXPERIMENT_DIR / "topology.ini").write_text(topology, encoding="utf-8")
    profile = {
        "description": "Configuration manifest, not proof that route generation completed",
        "satellites": 1584, "planes": 72, "satellites_per_plane": 22,
        "altitude_km": 550, "inclination_degrees": 53,
        "mobility_update_seconds": 0.1, "handover_update_seconds": 15,
        "duration_seconds": sim_time_seconds,
        "cities": CITY_COORDINATES, "extra_ground_stations": extra_ground_stations,
        "pairs": PAIR_DEFINITIONS, "path_count": PATH_COUNT,
        "snapshot_set": K_PATH_SNAPSHOT_SET, "max_rtt_spread_ms": MAX_RTT_SPREAD_MS,
    }
    (EXPERIMENT_DIR / "routing-profile.json").write_text(json.dumps(profile, indent=2) + "\n")
    generate_visualization_ini(extra_ground_stations, sim_time_seconds)
    print(f"Generated {INI_FILE}")
    return INI_FILE


def parse_args():
    parser = argparse.ArgumentParser(description="Generate shared LEO routing and visualization configurations")
    parser.add_argument("--sim-time", type=float, default=300, help="Simulation duration in seconds (default: 300)")
    return parser.parse_args()


if __name__ == "__main__":
    arguments = parse_args()
    generate_ini(arguments.sim_time)
