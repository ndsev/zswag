#!/usr/bin/env python3

"""Run the source-tree OAuth2 mock server against an installed zswag wheel."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import zserio


def working_dir() -> Path:
    return Path(__file__).resolve().parent / "oauth2_mock"


def prepare_generated_api() -> Path:
    oauth_dir = working_dir()
    zserio.generate(
        zs_dir=str(oauth_dir),
        main_zs_file="oauth_test.zs",
        gen_dir=str(oauth_dir),
        extra_args=["-withTypeInfoCode"],
    )
    return oauth_dir


def main() -> int:
    oauth_dir = prepare_generated_api()
    oauth_parent_dir = oauth_dir.parent
    if str(oauth_parent_dir) not in sys.path:
        sys.path.insert(0, str(oauth_parent_dir))

    from oauth2_mock.server import run_server

    parser = argparse.ArgumentParser(
        description="OAuth2 Mock Server for testing OAuth 1.0 signature and HTTP Basic Auth"
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    run_server(host=args.host, port=args.port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
