#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess

from routingSupport import (
    EXPERIMENT_DIR,
    REPO_ROOT,
    SAMPLES_ROOT,
    common_ned_path,
    load_libraries,
    tool_path,
)
from generateRoutingExamples import (
    PAIR_DEFINITIONS,
    POLICIES,
    TOPOLOGIES,
    VISUALIZATION_INI_FILE,
    generate_visualization_ini,
    load_ground_stations,
    topology_config_name,
)


def resolve_pair(value: str) -> tuple[int, str]:
    if value.isdigit():
        pair_number = int(value)
        if 1 <= pair_number <= len(PAIR_DEFINITIONS):
            pair = PAIR_DEFINITIONS[pair_number - 1]
            return pair_number - 1, pair[0]

    normalized = value.lower().replace("-", "").replace("_", "").replace(" ", "")
    for index, (key, source, destination, _, _) in enumerate(PAIR_DEFINITIONS):
        aliases = {
            key.lower(),
            f"{source}to{destination}".lower().replace(" ", ""),
        }
        if normalized in aliases:
            return index, key

    choices = f"a pair number from 1 to {len(PAIR_DEFINITIONS)} or a station-pair name"
    raise argparse.ArgumentTypeError(f"unknown pair '{value}'; choose {choices}")


def resolve_policy(value: str) -> str:
    normalized = value.lower().replace("-", "").replace("_", "").replace(" ", "")
    visualization_policies = ("ShortestPath", *POLICIES)
    for policy in visualization_policies:
        if normalized == policy.lower():
            return policy
    raise argparse.ArgumentTypeError(
        f"unknown policy '{value}'; choose {', '.join(visualization_policies)}"
    )


def parse_args():
    parser = argparse.ArgumentParser(
        description="Open the display-only 3D K-shortest-path viewer in Qtenv"
    )
    parser.add_argument(
        "--policy", type=resolve_policy, default="Unrestricted",
        help="ShortestPath for only the lowest-delay route, or Unrestricted, Shared1-Shared5, EdgeDisjoint",
    )
    parser.add_argument(
        "--pair",
        type=resolve_pair,
        default=resolve_pair("1"),
        help=f"Pair number (1-{len(PAIR_DEFINITIONS)}) or name, such as SanDiegoToShanghai",
    )
    parser.add_argument("--sim-time", type=float, default=300)
    parser.add_argument(
        "--topology", choices=TOPOLOGIES, default="ISL",
        help="Load ISL routes or saved GroundRelay routes with ISLs disabled",
    )
    parser.add_argument(
        "--no-isls",
        dest="show_isls",
        action="store_false",
        help="Hide the ISL background layer only; use --topology GroundRelay to change routing",
    )
    parser.add_argument(
        "--skip-generate",
        action="store_true",
        help="Reuse the existing visualization INI",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.skip_generate:
        generate_visualization_ini(load_ground_stations(), args.sim_time)
    if not VISUALIZATION_INI_FILE.is_file():
        raise FileNotFoundError(
            f"Missing {VISUALIZATION_INI_FILE}; run without --skip-generate first"
        )

    pair_index, pair_name = args.pair
    command = [
        tool_path("opp_run"),
        "-r",
        "0",
        "-m",
        "-u",
        "Qtenv",
        "-f",
        VISUALIZATION_INI_FILE.name,
        "-c",
        topology_config_name(f"View_{args.policy}", args.topology),
        "-n",
        common_ned_path(),
        f"--image-path={SAMPLES_ROOT / 'inet4.5' / 'images'}",
        f"--*.pathVisualizer.pairIndex={pair_index}",
        f"--*.pathVisualizer.showInterSatelliteLinks={'true' if args.show_isls and args.topology == 'ISL' else 'false'}",
        "-l",
        str(REPO_ROOT / "lib" / "oppqtenv-osg"),
    ]
    for library in load_libraries():
        command.extend(["-l", str(library)])

    print(f"Opening {args.topology}, {args.policy}, pair {pair_index + 1}: {pair_name}")
    print("$ " + " ".join(command))
    return subprocess.call(command, cwd=EXPERIMENT_DIR)


if __name__ == "__main__":
    raise SystemExit(main())
