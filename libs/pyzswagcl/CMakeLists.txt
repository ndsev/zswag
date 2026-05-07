project(pyzswagcl)

set(ZSWAG_RUNTIME_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/python-packages/zswag")

add_custom_target(zswag-python-package
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${ZSWAG_RUNTIME_PACKAGE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${ZSWAG_RUNTIME_PACKAGE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/libs/zswag/__init__.py"
    "${ZSWAG_RUNTIME_PACKAGE_DIR}/__init__.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/libs/zswag/app.py"
    "${ZSWAG_RUNTIME_PACKAGE_DIR}/app.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/libs/zswag/doc.py"
    "${ZSWAG_RUNTIME_PACKAGE_DIR}/doc.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/libs/zswag/gen.py"
    "${ZSWAG_RUNTIME_PACKAGE_DIR}/gen.py"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/libs/zswag/reflect.py"
    "${ZSWAG_RUNTIME_PACKAGE_DIR}/reflect.py"
  COMMENT "Preparing runtime Python package files for the zswag wheel")

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
  NAME zswag
  AUTHOR "Navigation Data Standard e.V."
  URL "https://github.com/ndsev/zswag"
  VERSION "${ZSWAG_VERSION}"
  PYTHON_REQUIRES ">=3.10"
  DESCRIPTION "OpenAPI service transport layer for zserio."
  README_PATH "${CMAKE_SOURCE_DIR}/README.md"
  TARGET_DEPENDENCIES
    zswagcl
  MODULE_DEPENDENCIES
    "connexion~=2.14.2"
    requests
    "zserio<3.0.0"
    pyyaml
    openapi-spec-validator
  DEPLOY_FILES
    ${ZLIB_DEPLOY_FILES}
  PYTHON_PACKAGE_DIRS
    "${ZSWAG_RUNTIME_PACKAGE_DIR}")

add_dependencies(pyzswagcl-copy-files zswag-python-package)
