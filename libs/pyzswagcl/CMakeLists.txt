project(pyzswagcl)

pybind11_add_module(pyzswagcl
  MODULE
    py-zswagcl.cpp
    py-openapi-client.cpp
    py-openapi-client.h)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(pyzswagcl PUBLIC /permissive-)
endif()

target_link_libraries(pyzswagcl
  PRIVATE
    zswagcl
    httpcl)

target_compile_features(pyzswagcl
  INTERFACE
    cxx_std_17)

# Platform-specific zlib file inclusion
# This ensures the repair tools can find the necessary library files and symlinks
if(APPLE)
  # Use TARGET_FILE to get the actual zlib library path dynamically
  # Also include the common symlinks that should exist
  set(ZLIB_DEPLOY_FILES
    "$<TARGET_FILE:zlib>"
    "${CMAKE_BINARY_DIR}/bin/libz.1.dylib"
    "${CMAKE_BINARY_DIR}/bin/libz.dylib")
elseif(UNIX)
  # Linux - use TARGET_FILE for the main library and common symlinks
  set(ZLIB_DEPLOY_FILES
    "$<TARGET_FILE:zlib>"
    "${CMAKE_BINARY_DIR}/bin/libz.so.1"
    "${CMAKE_BINARY_DIR}/bin/libz.so")
elseif(WIN32)
  # Windows - use TARGET_FILE for MSVC, manual paths for MinGW
  if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Use TARGET_FILE to get the actual DLL path regardless of configuration or version
    set(ZLIB_DEPLOY_FILES "$<TARGET_FILE:zlib>")
  else()
    # MinGW or other Windows compilers - check common DLL names
    set(ZLIB_DEPLOY_FILES
      "${CMAKE_BINARY_DIR}/bin/zlib.dll"
      "${CMAKE_BINARY_DIR}/bin/zlib1.dll")
  endif()
endif()

add_wheel(pyzswagcl
  AUTHOR "Navigation Data Standard e.V."
  URL "https://github.com/ndsev/zswag"
  VERSION "${ZSWAG_VERSION}"
  DESCRIPTION "Python bindings for the zswag client library."
  TARGET_DEPENDENCIES
    zswagcl httpcl
  DEPLOY_FILES
    ${ZLIB_DEPLOY_FILES})

