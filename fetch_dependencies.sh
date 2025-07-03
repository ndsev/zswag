
#!/bin/bash

# Create a temporary directory for the build
BUILD_DIR=$(mktemp -d)
echo "Using temporary build directory: $BUILD_DIR"

# Change to the build directory
cd "$BUILD_DIR"

# Configure CMake to fetch all dependencies
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=OFF -DFETCHCONTENT_SOURCE_DIR_OPENSSL="$BUILD_DIR/_deps/src/openssl" \
  -DFETCHCONTENT_SOURCE_DIR_ZLIB="$BUILD_DIR/_deps/src/zlib" \
  -DFETCHCONTENT_SOURCE_DIR_SPDLOG="$BUILD_DIR/_deps/src/spdlog" \
  -DFETCHCONTENT_SOURCE_DIR_YAML_CPP="$BUILD_DIR/_deps/src/yaml-cpp" \
  -DFETCHCONTENT_SOURCE_DIR_STX="$BUILD_DIR/_deps/src/stx" \
  -DFETCHCONTENT_SOURCE_DIR_SPEEDYJ="$BUILD_DIR/_deps/src/speedyj" \
  -DFETCHCONTENT_SOURCE_DIR_CATCH2="$BUILD_DIR/_deps/src/catch2" \
  -DFETCHCONTENT_SOURCE_DIR_HTTPLIB="$BUILD_DIR/_deps/src/httplib" \
  -DFETCHCONTENT_SOURCE_DIR_ZSERIO_CMAKE_HELPER="$BUILD_DIR/_deps/src/zserio-cmake-helper" \
  -DFETCHCONTENT_SOURCE_DIR_KEYCHAIN="$BUILD_DIR/_deps/src/keychain" \
  -DFETCHCONTENT_SOURCE_DIR_PYBIND11="$BUILD_DIR/_deps/src/pybind11" \
  -DFETCHCONTENT_SOURCE_DIR_PYTHON_CMAKE_WHEEL="$BUILD_DIR/_deps/src/python-cmake-wheel" \
  ..

# Copy the fetched dependencies to the _deps directory in the project root
mkdir -p "$(cd ..; pwd)/_deps/src"
cp -r "$BUILD_DIR/_deps/src/openssl" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/zlib" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/spdlog" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/yaml-cpp" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/stx" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/speedyj" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/catch2" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/httplib" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/zserio-cmake-helper" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/keychain" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/pybind11" "$(cd ..; pwd)/_deps/src/"
cp -r "$BUILD_DIR/_deps/src/python-cmake-wheel" "$(cd ..; pwd)/_deps/src/"

# Clean up
cd ..
rm -rf "$BUILD_DIR"

echo "Dependencies fetched and stored in _deps directory"
echo "You can now build with FETCHCONTENT_FULLY_DISCONNECTED=ON"
