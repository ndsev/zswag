cmake_minimum_required(VERSION 3.22.3) # to build without conan

include(FetchContent)

# Provide an option to disable Conan, defaulting to using Conan
option(ZSWAG_WITH_CONAN "Build with Conan dependency management" ON)

if(ZSWAG_WITH_CONAN)
  # Upgrade the required CMake version for Conan
  cmake_minimum_required(VERSION 3.24)
  if(NOT CONAN_PROVIDER_INCLUDED)
    set(CONAN_PROVIDER_INCLUDED true)
    FetchContent_Declare(
      conan_provider
      GIT_REPOSITORY https://github.com/Klebert-Engineering/cmake-conan
      GIT_TAG zswag)
    FetchContent_MakeAvailable(conan_provider)
    set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES "${conan_provider_SOURCE_DIR}/conan_provider.cmake")
  endif()
endif()

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
# dependencies

if(NOT TARGET OpenSSL)
  find_package(OpenSSL CONFIG REQUIRED)
endif()

if(ZSWAG_KEYCHAIN_SUPPORT)
  find_package(keychain CONFIG REQUIRED)
endif()

if (ZSWAG_BUILD_WHEELS)
  FetchContent_Declare(python-cmake-wheel
  GIT_REPOSITORY "https://github.com/Klebert-Engineering/python-cmake-wheel"
  GIT_TAG        "v0.9.0"
  GIT_SHALLOW    ON)

  FetchContent_GetProperties(python-cmake-wheel)

  if(NOT python-cmake-wheel_POPULATED)
    FetchContent_Populate(python-cmake-wheel)
    set(CMAKE_MODULE_PATH "${python-cmake-wheel_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
  endif()

  if (NOT TARGET wheel)
    set(Python3_FIND_STRATEGY LOCATION)
    include(python-wheel)
    set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
  endif()
endif()

if (NOT TARGET yaml-cpp::yaml-cpp)
  FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY "https://github.com/jbeder/yaml-cpp.git"
    GIT_TAG        "yaml-cpp-0.7.0"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(yaml-cpp)
endif()

if (NOT TARGET stx)
  FetchContent_Declare(stx
    GIT_REPOSITORY "https://github.com/Klebert-Engineering/stx.git"
    GIT_TAG        "main"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(stx)
endif()

if (NOT TARGET speedyj)
  FetchContent_Declare(speedyj
    GIT_REPOSITORY "https://github.com/Klebert-Engineering/speedyj.git"
    GIT_TAG        "master"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(speedyj)
endif()

if (NOT TARGET Catch2::Catch2)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY "https://github.com/catchorg/Catch2.git"
    GIT_TAG        "v3.4.0"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(Catch2)
endif()

if (NOT TARGET httplib::httplib)
  if (NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB CONFIG REQUIRED)
  endif ()
  set (HTTPLIB_IS_USING_ZLIB TRUE)
  FetchContent_Declare(httplib
    GIT_REPOSITORY "https://github.com/yhirose/cpp-httplib.git"
    GIT_TAG        "v0.15.3"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(httplib)
  target_compile_definitions(httplib
    INTERFACE
      CPPHTTPLIB_OPENSSL_SUPPORT)
  target_link_libraries(
    httplib INTERFACE OpenSSL::SSL ZLIB::ZLIB)
endif()

if(ZSWAG_BUILD_WHEELS AND NOT TARGET pybind11::pybind11)
  find_package(pybind11 REQUIRED)
endif()

if (NOT TARGET zserio-cmake-helper)
  set(ZSERIO_VERSION "2.13.0")
  FetchContent_Declare(zserio-cmake-helper
    GIT_REPOSITORY "https://github.com/Klebert-Engineering/zserio-cmake-helper.git"
    GIT_TAG        "main"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(zserio-cmake-helper)
  if (NOT TARGET ZserioCppRuntime)
    add_zserio_cpp_runtime()
  endif()
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





