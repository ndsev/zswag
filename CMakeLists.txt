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

# OpenSSL - use system installation instead of submodule
set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

# zlib
add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)

# spdlog
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

# yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp EXCLUDE_FROM_ALL)

# stx
add_subdirectory(third_party/stx EXCLUDE_FROM_ALL)

# speedyj
add_subdirectory(third_party/speedyj EXCLUDE_FROM_ALL)

# Catch2 (for testing)
if(ZSWAG_ENABLE_TESTING)
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_HELPERS OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/catch2 EXCLUDE_FROM_ALL)
endif()

# httplib
set(HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/httplib EXCLUDE_FROM_ALL)
# Configure httplib with OpenSSL and zlib support
target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)

# keychain (if enabled)
if(ZSWAG_KEYCHAIN_SUPPORT)
  add_subdirectory(third_party/keychain EXCLUDE_FROM_ALL)
endif()

# pybind11 (if building wheels)
if(ZSWAG_BUILD_WHEELS)
  set(PYBIND11_TEST OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/pybind11 EXCLUDE_FROM_ALL)
endif()

# python-cmake-wheel
if (ZSWAG_BUILD_WHEELS)
  set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/python-cmake-wheel" ${CMAKE_MODULE_PATH})
  
  if (NOT TARGET wheel)
    set(Python3_FIND_STRATEGY LOCATION)
    include(python-wheel)
    set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
  endif()
endif()

# zserio-cmake-helper
set(ZSERIO_VERSION "2.13.0")
add_subdirectory(third_party/zserio-cmake-helper EXCLUDE_FROM_ALL)
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





