#!/usr/bin/env python3
"""
Determine version for zswag builds.

For regular builds (push/PR): Uses setuptools_scm directly.
For manual snapshots: Uses setuptools_scm output without local version identifiers,
ensuring PyPI compatibility while maintaining traceability via release tags.
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path


def get_setuptools_scm_version():
    """Get version from setuptools_scm."""
    try:
        result = subprocess.run(
            [
                sys.executable,
                "-c",
                "import setuptools_scm; print(setuptools_scm.get_version(local_scheme='no-local-version'))"
            ],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to get version from setuptools_scm", file=sys.stderr)
        print(f"stdout: {e.stdout}", file=sys.stderr)
        print(f"stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)


def validate_pep440(version):
    """Validate that version conforms to PEP 440."""
    try:
        from packaging.version import Version
        Version(version)
    except ImportError:
        # packaging not available, skip validation
        print("Warning: 'packaging' module not available, skipping PEP 440 validation", file=sys.stderr)
    except Exception as e:
        print(f"Error: Version '{version}' is not PEP 440 compliant: {e}", file=sys.stderr)
        sys.exit(1)


def write_output(version):
    """Write version to GITHUB_OUTPUT."""
    github_output = os.environ.get("GITHUB_OUTPUT")
    if not github_output:
        print(f"Version: {version}")
        return

    try:
        with Path(github_output).open("a", encoding="utf-8") as fh:
            fh.write(f"version={version}\n")
        print(f"Version: {version}")
    except Exception as e:
        print(f"Error: Failed to write to GITHUB_OUTPUT: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Determine version for zswag builds")
    parser.add_argument("--event", required=True, help="GitHub event name (push, pull_request, workflow_dispatch, etc.)")
    parser.add_argument("--ref", help="Git ref being built (branch, tag, or commit)")
    args = parser.parse_args()

    # Get base version from setuptools_scm
    version = get_setuptools_scm_version()

    # For manual snapshots, use clean setuptools_scm version
    # Traceability comes from the release tag name (snapshot-branch-commit)
    # This keeps versions PyPI-compatible

    # Validate PEP 440 compliance
    validate_pep440(version)

    # Write to output
    write_output(version)


if __name__ == "__main__":
    main()
