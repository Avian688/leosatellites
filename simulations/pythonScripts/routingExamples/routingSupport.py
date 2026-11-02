#!/usr/bin/env python3

from __future__ import annotations

import os
import signal
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
SIMULATIONS_DIR = SCRIPT_DIR.parents[1]
PROJECT_ROOT = SIMULATIONS_DIR.parent
SAMPLES_ROOT = PROJECT_ROOT.parent
REPO_ROOT = SAMPLES_ROOT.parent
EXPERIMENT_DIR = SIMULATIONS_DIR / "examples" / "routing"
RESULTS_DIR = EXPERIMENT_DIR / "results"
LOG_DIR = SIMULATIONS_DIR / "logs" / "routingExamples"
INI_FILE = EXPERIMENT_DIR / "routing.ini"

ACTIVE_PROCESSES: set[subprocess.Popen] = set()
ACTIVE_PROCESSES_LOCK = threading.Lock()


@dataclass(frozen=True)
class SimulationConfig:
    config_name: str
    require_vector: bool = True


def default_cores(maximum_tasks: int) -> int:
    configured = os.environ.get("EXPERIMENT_CORES")
    available = int(configured) if configured is not None else (os.cpu_count() or 1)
    return max(1, min(maximum_tasks, available))


def tool_path(name: str) -> str:
    configured = os.environ.get(name.upper())
    if configured:
        return configured
    bundled = REPO_ROOT / "bin" / name
    if bundled.exists():
        return str(bundled)
    return name


def common_ned_path() -> str:
    paths = [
        SIMULATIONS_DIR,
        SAMPLES_ROOT / "leosatellites" / "src",
        SAMPLES_ROOT / "os3" / "src",
        SAMPLES_ROOT / "inet4.5" / "src",
    ]
    return ":".join(str(path) for path in paths)


def load_libraries() -> list[Path]:
    return [
        SAMPLES_ROOT / "inet4.5" / "src" / "INET",
        SAMPLES_ROOT / "os3" / "src" / "os3",
        SAMPLES_ROOT / "leosatellites" / "src" / "leosatellites",
    ]


def simulation_command(config: SimulationConfig) -> list[str]:
    command = [
        tool_path("opp_run"),
        "-r",
        "0",
        "-m",
        "-u",
        "Cmdenv",
        "-f",
        INI_FILE.name,
        "-c",
        config.config_name,
        "-n",
        common_ned_path(),
        f"--image-path={SAMPLES_ROOT / 'inet4.5' / 'images'}",
    ]
    for library in load_libraries():
        command.extend(["-l", str(library)])
    return command


def result_path(config: SimulationConfig, suffix: str) -> Path:
    return RESULTS_DIR / f"{config.config_name}{suffix}"


def vector_exists(config: SimulationConfig) -> bool:
    path = result_path(config, "-#0.vec")
    return path.is_file() and path.stat().st_size > 0


def clean_results(config: SimulationConfig) -> None:
    for suffix in ("-#0.vec", "-#0.vci", "-#0.sca"):
        result_path(config, suffix).unlink(missing_ok=True)


def terminate_process_group(process: subprocess.Popen) -> None:
    if process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    except PermissionError:
        process.terminate()
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except PermissionError:
            process.kill()
        process.wait()


def register_process(process: subprocess.Popen) -> None:
    with ACTIVE_PROCESSES_LOCK:
        ACTIVE_PROCESSES.add(process)


def unregister_process(process: subprocess.Popen) -> None:
    with ACTIVE_PROCESSES_LOCK:
        ACTIVE_PROCESSES.discard(process)


def terminate_all_active_processes() -> None:
    with ACTIVE_PROCESSES_LOCK:
        processes = list(ACTIVE_PROCESSES)
    for process in processes:
        terminate_process_group(process)


def handle_termination_signal(signum, _frame) -> None:
    print(f"\nReceived signal {signum}; cancelling experiment runner...", file=sys.stderr)
    raise KeyboardInterrupt


def install_signal_handlers() -> None:
    signal.signal(signal.SIGINT, handle_termination_signal)
    signal.signal(signal.SIGTERM, handle_termination_signal)


def run_logged_command(
    command: list[str],
    log_path: Path,
    timeout_seconds: float | None,
) -> tuple[int, bool]:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    timed_out = False
    with log_path.open("w", encoding="utf-8") as log:
        log.write("$ " + " ".join(command) + "\n\n")
        log.flush()
        process = subprocess.Popen(
            command,
            cwd=str(EXPERIMENT_DIR),
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        register_process(process)
        try:
            return_code = process.wait(timeout=timeout_seconds)
        except subprocess.TimeoutExpired:
            timed_out = True
            terminate_process_group(process)
            return_code = process.returncode if process.returncode is not None else 124
        except BaseException:
            terminate_process_group(process)
            raise
        finally:
            unregister_process(process)

        elapsed = time.monotonic() - started
        if timed_out:
            log.write(f"\nTimed out after {elapsed:.2f} seconds\n")
        log.write(f"\nExit code: {return_code}\n")
        log.write(f"Elapsed seconds: {elapsed:.2f}\n")
    return return_code, timed_out


def run_config(
    config: SimulationConfig,
    phase: str,
    timeout_seconds: float | None,
    resume: bool,
) -> tuple[SimulationConfig, bool, int, Path]:
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    if resume and config.require_vector and vector_exists(config):
        return config, True, 0, Path()

    clean_results(config)
    log_path = LOG_DIR / phase / f"{config.config_name}.log"
    return_code, _ = run_logged_command(
        simulation_command(config),
        log_path,
        timeout_seconds,
    )
    outputs_ok = not config.require_vector or vector_exists(config)
    return config, return_code == 0 and outputs_ok, return_code, log_path


def run_simulation_configs(
    configs: list[SimulationConfig],
    phase: str,
    cores: int,
    retries: int,
    timeout_seconds: float | None,
    resume: bool = False,
) -> None:
    if not INI_FILE.is_file():
        raise FileNotFoundError(f"Missing generated INI: {INI_FILE}")
    if cores < 1:
        raise ValueError("--cores must be at least 1")
    if retries < 0:
        raise ValueError("--retries must not be negative")

    pending = list(configs)
    attempts = retries + 1
    failure_lines: list[str] = []
    for attempt in range(1, attempts + 1):
        if not pending:
            return

        workers = min(cores, len(pending))
        print(f"{phase}: {len(pending)} task(s), attempt {attempt}/{attempts}, {workers} worker(s)")
        failures: list[SimulationConfig] = []
        failure_lines = []
        executor = ThreadPoolExecutor(max_workers=workers)
        futures = {}
        interrupted = False
        try:
            futures = {
                executor.submit(run_config, config, phase, timeout_seconds, resume): config
                for config in pending
            }
            for future in as_completed(futures):
                config, ok, return_code, log_path = future.result()
                if ok:
                    print(f"  ok: {config.config_name}")
                else:
                    failures.append(config)
                    failure = f"{config.config_name} (exit {return_code}, log: {log_path})"
                    failure_lines.append(failure)
                    print(f"  failed: {failure}")
        except KeyboardInterrupt:
            interrupted = True
            for future in futures:
                future.cancel()
            terminate_all_active_processes()
            executor.shutdown(wait=False, cancel_futures=True)
            raise
        finally:
            if not interrupted:
                executor.shutdown(wait=True)

        pending = failures
        resume = False
        if pending and attempt < attempts:
            print(f"Retrying {len(pending)} failed task(s)")

    if pending:
        raise RuntimeError(f"{phase} failed:\n  " + "\n  ".join(failure_lines))
