#!/usr/bin/env python3

"""Run the source-tree calculator test helpers against an installed zswag wheel."""

from __future__ import annotations

import sys
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


def main() -> int:
    calc_dir = prepare_generated_api()
    if str(calc_dir) not in sys.path:
        sys.path.insert(0, str(calc_dir))

    import calculator.api as calculator
    import client as calc_client
    import server as calc_server

    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    host, port = sys.argv[2].split(":") if len(sys.argv) > 2 and ":" in sys.argv[2] else ("localhost", "5000")

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

    print("Usage: python run_calc.py {server|client|path} [host:port]")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
