

# FetchContent Migration Guide

This project has migrated from Git submodules to CMake FetchContent for managing dependencies.

## Benefits of the new approach

1. **Offline builds**: Support for offline builds using `FETCHCONTENT_FULLY_DISCONNECTED=ON`
2. **Local overrides**: Easy local overrides of dependencies using `FETCHCONTENT_SOURCE_DIR_<NAME>` CMake variables
3. **Simplified CMake configuration**: All dependency declarations are centralized in `cmake/Dependencies.cmake`
4. **No Git operations required**: Dependencies are managed by CMake, not Git

## Usage Instructions

### Basic Build

To build the project with default settings (fetching dependencies from Git as needed):

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Offline Build

To build the project in offline mode (after dependencies have been prefetched):

```bash
mkdir build
cd build
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON ..
cmake --build .
```

### Local Overrides

To use a local copy of a dependency instead of fetching it from Git:

```bash
mkdir build
cd build
cmake -DFETCHCONTENT_SOURCE_DIR_OPENSSL=/path/to/local/openssl ..
cmake --build .
```

You can specify local overrides for any dependency by setting `FETCHCONTENT_SOURCE_DIR_<NAME>` to the path of the local source.

### Prefetching Dependencies

To prefetch all dependencies for offline use:

```bash
./fetch_dependencies.sh
```

This script will download all dependencies and store them in the `_deps` directory. You can then copy this directory to another machine for offline builds.

## Dependency List

The following dependencies are managed by FetchContent:

- openssl
- zlib
- spdlog
- yaml-cpp
- stx
- speedyj
- catch2
- httplib
- zserio-cmake-helper
- keychain
- pybind11
- python-cmake-wheel

## CMake Variables

The following CMake variables are available:

- `FETCHCONTENT_FULLY_DISCONNECTED`: Set to `ON` to enable offline builds
- `FETCHCONTENT_SOURCE_DIR_<NAME>`: Set to a local path to override a specific dependency

## Troubleshooting

If you encounter issues with FetchContent, try the following:

1. Clean the build directory and start fresh
2. Make sure you have internet access (unless using offline mode)
3. Check that your local overrides are correctly specified

