#!/usr/bin/env python3
"""
Validate that the setuptools_scm version matches the CMake version.

This script is used in the CI/CD pipeline to ensure version consistency
between the CMake configuration and Git tags before deployment to PyPI.

The script compares:
- Base version from CMakeLists.txt (e.g., "1.7.2")
- Base version from setuptools_scm (e.g., "1.7.2" from tag or "1.7.2.dev3" from commit)

If the base versions don't match, the build fails to prevent deploying
mismatched versions. This ensures that:
- Release tags (v1.7.2) match the CMake version (1.7.2)
- Dev builds from main branch use the correct base version
"""
import re
import sys
import subprocess


def get_cmake_version():
    """Extract ZSWAG_VERSION from CMakeLists.txt"""
    with open('CMakeLists.txt', 'r') as f:
        content = f.read()
    
    match = re.search(r'set\(ZSWAG_VERSION\s+([0-9.]+)\)', content)
    if not match:
        raise ValueError("Could not find ZSWAG_VERSION in CMakeLists.txt")
    
    return match.group(1)


def get_scm_version():
    """Get version from setuptools_scm"""
    result = subprocess.run(
        [sys.executable, '-m', 'setuptools_scm'],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        raise ValueError(f"setuptools_scm failed: {result.stderr}")
    
    return result.stdout.strip()


def get_base_version(version):
    """Extract base version (without dev/post parts)"""
    # Remove everything after + (local version)
    version = version.split('+')[0]
    # Remove dev/post/rc parts
    parts = version.split('.')
    if len(parts) > 3:
        # Check if last part is dev/post/rc
        if parts[3].startswith('dev') or parts[3].startswith('post'):
            parts = parts[:3]
    return '.'.join(parts[:3])


def main():
    cmake_version = get_cmake_version()
    scm_version = get_scm_version()
    scm_base_version = get_base_version(scm_version)
    
    print(f"CMake version: {cmake_version}")
    print(f"SetupTools SCM version: {scm_version}")
    print(f"SetupTools SCM base version: {scm_base_version}")
    
    if cmake_version != scm_base_version:
        print(f"\nERROR: Version mismatch!")
        print(f"  CMake:    {cmake_version}")
        print(f"  SCM base: {scm_base_version}")
        sys.exit(1)
    
    print("\nVersion validation passed!")
    
    # Output for GitHub Actions
    if 'GITHUB_OUTPUT' in os.environ:
        with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
            f.write(f"version={scm_version}\n")
            f.write(f"base_version={scm_base_version}\n")
            f.write(f"cmake_version={cmake_version}\n")


if __name__ == '__main__':
    import os
    main()