cmake_minimum_required(VERSION 3.22.3)

project(zswag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow version override for development builds.
# This enables PEP 440 compliant dev versions (e.g., 1.7.3.dev14) from setuptools_scm
# without requiring CMakeLists.txt updates between releases.
# For official releases, this value should match the git tag.
if(NOT DEFINED ZSWAG_VERSION)
  set(ZSWAG_VERSION 1.8.0)
endif()

option(ZSWAG_BUILD_WHEELS "Enable zswag whl-output to WHEEL_DEPLOY_DIRECTORY." ON)
option(ZSWAG_KEYCHAIN_SUPPORT "Enable zswag keychain support." ON)
option(ZSWAG_ENABLE_TESTING "Enable testing for the project" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  message (STATUS "Testing will be enabled as zswag is the top-level project.")
  set (ZSWAG_ENABLE_TESTING ON CACHE BOOL "By default, enable testing if this is the main project")
endif()

if (ZSWAG_ENABLE_TESTING)
  enable_testing()
endif()

if (NOT MSVC)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_FLAGS -fPIC)
endif()

if (NOT ZSWAG_DEPLOY_DIR)
  set (ZSWAG_DEPLOY_DIR "${CMAKE_BINARY_DIR}/bin")
endif()
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")

##############
# Third-party dependencies via git submodules

# OpenSSL - build from source using ExternalProject
include(ExternalProject)

# Determine platform-specific OpenSSL configuration
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/Configure VC-WIN64A)
    else()
        set(OPENSSL_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/Configure VC-WIN32)
    endif()
    set(OPENSSL_BUILD_COMMAND nmake)
    set(OPENSSL_INSTALL_COMMAND nmake install)
    set(OPENSSL_LIB_PREFIX "")
    set(OPENSSL_LIB_SUFFIX ".lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(OPENSSL_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/Configure darwin64-arm64-cc)
    else()
        set(OPENSSL_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/Configure darwin64-x86_64-cc)
    endif()
    set(OPENSSL_BUILD_COMMAND make)
    set(OPENSSL_INSTALL_COMMAND make install_sw)
    set(OPENSSL_LIB_PREFIX "lib")
    set(OPENSSL_LIB_SUFFIX ".a")
else() # Linux and other Unix-like systems
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/Configure linux-x86_64)
    else()
        set(OPENSSL_CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl/Configure linux-x86)
    endif()
    set(OPENSSL_BUILD_COMMAND make)
    set(OPENSSL_INSTALL_COMMAND make install_sw)
    set(OPENSSL_LIB_PREFIX "lib")
    set(OPENSSL_LIB_SUFFIX ".a")
endif()

# Set OpenSSL install directory
set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps/openssl-install)

# Build OpenSSL using ExternalProject
ExternalProject_Add(openssl_build
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl
    CONFIGURE_COMMAND ${OPENSSL_CONFIGURE_COMMAND}
                      --prefix=${OPENSSL_INSTALL_DIR}
                      --openssldir=${OPENSSL_INSTALL_DIR}/ssl
                      no-shared
                      no-tests
    BUILD_COMMAND ${OPENSSL_BUILD_COMMAND}
    INSTALL_COMMAND ${OPENSSL_INSTALL_COMMAND}
    BUILD_IN_SOURCE 1
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
)

# Create imported targets for OpenSSL::SSL and OpenSSL::Crypto
add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)

# Set target properties
set_target_properties(OpenSSL::SSL PROPERTIES
    IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
)

set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
)

# Add dependencies
add_dependencies(OpenSSL::SSL openssl_build)
add_dependencies(OpenSSL::Crypto openssl_build)

# Link SSL to Crypto
set_target_properties(OpenSSL::SSL PROPERTIES
    INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
)

# Platform-specific system libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(OpenSSL::Crypto PROPERTIES
        INTERFACE_LINK_LIBRARIES "ws2_32;gdi32;advapi32;crypt32;user32"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(OpenSSL::Crypto PROPERTIES
        INTERFACE_LINK_LIBRARIES "dl;pthread"
    )
endif()

# zlib
add_subdirectory(deps/zlib EXCLUDE_FROM_ALL)

# spdlog
add_subdirectory(deps/spdlog EXCLUDE_FROM_ALL)

# yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/yaml-cpp EXCLUDE_FROM_ALL)

# stx
add_subdirectory(deps/stx EXCLUDE_FROM_ALL)

# speedyj
add_subdirectory(deps/speedyj EXCLUDE_FROM_ALL)

# Catch2 (for testing)
if(ZSWAG_ENABLE_TESTING)
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
  add_subdirectory(deps/catch2 EXCLUDE_FROM_ALL)
endif()

# httplib
set(HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/httplib EXCLUDE_FROM_ALL)
# Configure httplib with OpenSSL and zlib support
target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)

# keychain
if (ZSWAG_KEYCHAIN_SUPPORT)
  add_subdirectory(deps/keychain EXCLUDE_FROM_ALL)
endif()

# pybind11
add_subdirectory(deps/pybind11 EXCLUDE_FROM_ALL)

# python-cmake-wheel
if (ZSWAG_BUILD_WHEELS)
  set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/python-cmake-wheel" ${CMAKE_MODULE_PATH})
  
  if (NOT TARGET wheel)
    set(Python3_FIND_STRATEGY LOCATION)
    include(python-wheel)
    set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
  endif()
endif()

# zserio-cmake-helper
set(ZSERIO_VERSION "2.13.0")
add_subdirectory(deps/zserio-cmake-helper EXCLUDE_FROM_ALL)
if (NOT TARGET ZserioCppRuntime)
  add_zserio_cpp_runtime()
endif()

##############
# libs

add_subdirectory(libs/httpcl)
add_subdirectory(libs/zswagcl)

##############
# wheels

if (ZSWAG_BUILD_WHEELS)
  add_subdirectory(libs/pyzswagcl)
  add_subdirectory(libs/zswag/test)

  add_custom_target(zswag-server-wheel
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_LIST_DIR}
    COMMAND
      ${Python3_EXECUTABLE} setup.py ${ZSWAG_VERSION} bdist_wheel -d "${WHEEL_DEPLOY_DIRECTORY}")
  add_dependencies(wheel zswag-server-wheel)
endif()





