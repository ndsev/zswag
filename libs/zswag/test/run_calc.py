#!/usr/bin/env python3

"""Run the source-tree calculator test helpers against an installed zswag wheel."""

from __future__ import annotations

import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

import zserio

import zswag


def working_dir() -> Path:
    return Path(__file__).resolve().parent / "calc"


def prepare_generated_api() -> Path:
    calc_dir = working_dir()
    zserio.generate(
        zs_dir=str(calc_dir),
        main_zs_file="calculator.zs",
        gen_dir=str(calc_dir),
        extra_args=["-withTypeInfoCode"],
    )
    return calc_dir


def wait_for_openapi(host: str, port: str, timeout_seconds: float = 60.0) -> int:
    """Wait until the calculator server is reachable before running clients.

    The wheel test launches the server as a background process. On slower
    Windows runners the fixed delay in the test harness is not always enough,
    so the integration test needs an explicit readiness probe.
    """
    url = f"http://{host}:{port}/openapi.json"
    deadline = time.monotonic() + timeout_seconds
    last_error: Exception | None = None

    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=1.0) as response:
                if response.status == 200:
                    print(f"Calculator server is ready at {url}")
                    return 0
        except (OSError, urllib.error.URLError) as error:
            last_error = error

        time.sleep(0.25)

    print(f"Timed out waiting for calculator server at {url}: {last_error}", file=sys.stderr)
    return 1


def main() -> int:
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    host, port = sys.argv[2].split(":") if len(sys.argv) > 2 and ":" in sys.argv[2] else ("localhost", "5000")

    if mode == "wait":
        return wait_for_openapi(host, port)

    calc_dir = prepare_generated_api()
    if str(calc_dir) not in sys.path:
        sys.path.insert(0, str(calc_dir))

    import calculator.api as calculator
    import client as calc_client
    import server as calc_server

    if mode == "client":
        calc_client.run(host, port)
        return 0

    if mode == "server":
        app = zswag.OAServer(
            controller_module=calc_server,
            service_type=calculator.Calculator.Service,
            yaml_path=str(calc_dir / "api.yaml"),
            zs_pkg_path=str(calc_dir),
        )
        app.run(host=host, port=port)
        return 0

    if mode == "path":
        print(calc_dir)
        return 0

    print("Usage: python run_calc.py {server|client|wait|path} [host:port]")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
