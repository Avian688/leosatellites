#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import sys

from routingSupport import (
    SimulationConfig,
    default_cores,
    install_signal_handlers,
    run_simulation_configs,
    terminate_all_active_processes,
)
from generateRoutingExamples import (
    EXPERIMENT_DIR, POLICIES, TOPOLOGIES, generate_ini, topology_config_name,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate shared LEO primary and K-path routing files"
    )
    parser.add_argument("--start-step", type=int, choices=(1, 2), default=1)
    parser.add_argument("--mode", choices=("shortest", "multiple"), default="multiple",
                        help="shortest saves primary routes; multiple also saves K-path catalogs")
    parser.add_argument("--with-viewer", action="store_true",
                        help="With shortest mode, also save the Unrestricted catalog needed by the 3D viewer")
    parser.add_argument("--end-step", type=int, choices=(1, 2), default=2)
    parser.add_argument("--sim-time", type=float, default=300, help="Saved routing duration in seconds")
    parser.add_argument(
        "--topology", choices=TOPOLOGIES, default="ISL",
        help="ISL links enabled, or GroundRelay through ground stations with ISLs disabled",
    )
    parser.add_argument("--policies", nargs="+", choices=list(POLICIES), default=list(POLICIES))
    parser.add_argument(
        "--cores",
        type=int,
        default=default_cores(len(POLICIES)),
        help="Maximum simultaneous K-path generators (default: EXPERIMENT_CORES or up to 7 CPUs)",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=int(os.environ.get("EXPERIMENT_RETRIES", "1")),
    )
    parser.add_argument(
        "--sim-timeout-seconds",
        type=float,
        default=float(os.environ.get("EXPERIMENT_SIM_TIMEOUT_SECONDS", str(8 * 60 * 60))),
        help="Wall-clock timeout per route-generation simulation (default: 8 hours)",
    )
    return parser.parse_args()


def main() -> int:
    install_signal_handlers()
    args = parse_args()
    if args.cores < 1 or args.retries < 0 or args.sim_timeout_seconds <= 0:
        raise ValueError("cores and timeout must be positive; retries must be non-negative")
    if args.mode == "shortest":
        if args.with_viewer:
            args.policies = ["Unrestricted"]
        else:
            args.end_step = 1
    if args.start_step > args.end_step:
        raise ValueError("--start-step must not exceed --end-step")

    generate_ini(args.sim_time)
    topology_tag = "ISL" if args.topology == "ISL" else "BP"
    print(f"Route store: {(EXPERIMENT_DIR / '../../leoSaves').resolve()} (topology {args.topology}, _{topology_tag} directory)")

    if args.start_step <= 1 <= args.end_step:
        print("Step 1/2: generating primary shortest-path routes")
        run_simulation_configs(
            [SimulationConfig(topology_config_name("GenerateShortestPaths", args.topology), require_vector=False)],
            topology_config_name("primary-routes", args.topology),
            1,
            args.retries,
            args.sim_timeout_seconds,
        )

    if args.start_step <= 2 <= args.end_step:
        print("Step 2/2: generating K-shortest-path policy catalogs")
        configs = [
            SimulationConfig(topology_config_name(f"Generate_{policy}", args.topology), require_vector=False)
            for policy in args.policies
        ]
        run_simulation_configs(
            configs,
            topology_config_name("k-path-routes", args.topology),
            args.cores,
            args.retries,
            args.sim_timeout_seconds,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        terminate_all_active_processes()
        print("Cancelled; active simulation process groups were stopped.", file=sys.stderr)
        raise SystemExit(130)
    except Exception as error:
        terminate_all_active_processes()
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
