




# Summary of Changes: Migration from Git Submodules to CMake FetchContent

This document summarizes the changes made to migrate the project from using Git submodules to CMake FetchContent for managing dependencies.

## Files Created/Modified

1. **cmake/Dependencies.cmake**: Centralized file containing all dependency declarations using FetchContent
2. **fetch_dependencies.sh**: Script to prefetch all dependencies for offline builds
3. **CMakeLists.txt**: Updated to use FetchContent instead of Git submodules
4. **README_FETCHCONTENT.md**: Documentation explaining the new approach and how to use it
5. **EXAMPLE_CMakeLists.txt**: Example showing how to use the FetchContent dependencies
6. **fetch_dependencies.sh.readme**: Documentation for the fetch_dependencies.sh script
7. **cmake/Dependencies.cmake.readme**: Documentation for the Dependencies.cmake file

## Key Features of the New Approach

1. **Offline builds**: Support for offline builds using `FETCHCONTENT_FULLY_DISCONNECTED=ON`
2. **Local overrides**: Easy local overrides of dependencies using `FETCHCONTENT_SOURCE_DIR_<NAME>` CMake variables
3. **Simplified CMake configuration**: All dependency declarations are centralized in `cmake/Dependencies.cmake`
4. **No Git operations required**: Dependencies are managed by CMake, not Git

## Benefits

1. **Improved build reproducibility**: Dependencies are pinned to specific versions
2. **Better offline support**: Dependencies can be prefetched and used in offline environments
3. **Simplified local development**: Easy to override dependencies with local versions
4. **Cleaner repository structure**: No more Git submodules to manage

## Usage Instructions

See **README_FETCHCONTENT.md** for detailed instructions on how to build the project with the new setup.

## Example

To build the project with default settings:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

To build in offline mode:

```bash
mkdir build
cd build
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON ..
cmake --build .
```

To use a local copy of OpenSSL:

```bash
mkdir build
cd build
cmake -DFETCHCONTENT_SOURCE_DIR_OPENSSL=/path/to/local/openssl ..
cmake --build .
```

## Migration Process

1. Created a new `cmake/Dependencies.cmake` file with FetchContent declarations for all dependencies
2. Updated `CMakeLists.txt` to use FetchContent instead of Git submodules
3. Adapted the OpenSSL build process to work with FetchContent
4. Created a script to prefetch dependencies for offline builds
5. Added documentation explaining the new approach and how to use it

## Next Steps

1. Test the new setup thoroughly to ensure all dependencies are correctly fetched and built
2. Update any CI/CD pipelines to use the new approach
3. Remove the old Git submodules from the repository (after thorough testing)

