cmake_minimum_required(VERSION 3.22.3)

project(zswag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow version override for development builds.
# This enables PEP 440 compliant dev versions (e.g., 1.7.3.dev14) from setuptools_scm
# without requiring CMakeLists.txt updates between releases.
# For official releases, this value should match the git tag.
if(NOT DEFINED ZSWAG_VERSION)
  set(ZSWAG_VERSION 1.9.0)
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
else()
  # Enable exception handling and suppress C4530 warnings on MSVC
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

if (NOT ZSWAG_DEPLOY_DIR)
  set (ZSWAG_DEPLOY_DIR "${CMAKE_BINARY_DIR}/bin")
endif()
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")

##############
# Dependencies via FetchContent

# Include centralized dependency management
include(cmake/Dependencies.cmake)

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





