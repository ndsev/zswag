cmake_minimum_required(VERSION 3.14)

if (NOT PROJECT_NAME)
  set (ZSWAG_ENABLE_TESTING ON)
endif()

project(zswag)
enable_testing()
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ZSWAG_VERSION 1.5.0)

option(ZSWAG_BUILD_WHEELS "Enable zswag whl-output to WHEEL_DEPLOY_DIRECTORY." ON)
option(ZSWAG_KEYCHAIN_SUPPORT "Enable zswag keychain support." ON)

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
# deps

find_package(OpenSSL REQUIRED)

if (ZSWAG_BUILD_WHEELS)
  if (NOT TARGET wheel)
    set(Python3_FIND_STRATEGY LOCATION)
    set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/python-cmake-wheel")
    include(python-wheel)
    set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
  endif()
endif()

if (NOT TARGET yaml-cpp)
  add_subdirectory(deps/yaml-cpp)
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

if (NOT TARGET spdlog)
  FetchContent_Declare(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog.git"
    GIT_TAG        "v1.x"
    GIT_SHALLOW    ON)
  FetchContent_MakeAvailable(spdlog)
endif()

if (NOT TARGET Catch2)
  add_subdirectory(deps/Catch2)
endif()

if (NOT TARGET httplib)
  add_subdirectory(deps/cpp-httplib)
  target_compile_definitions(httplib
    INTERFACE
    CPPHTTPLIB_OPENSSL_SUPPORT)
  target_link_libraries(httplib INTERFACE OpenSSL::SSL)
endif()

if (ZSWAG_KEYCHAIN_SUPPORT)
  if (NOT TARGET keychain)
    add_subdirectory(deps/keychain)
  endif()
endif()

if (ZSWAG_BUILD_WHEELS AND NOT TARGET pybind11)
  add_subdirectory(deps/pybind11)
endif()

if (NOT TARGET zserio-cmake-helper)
  set(ZSERIO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/deps/zserio")
  add_subdirectory(deps/zserio-cmake-helper)
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
