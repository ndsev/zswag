cmake_minimum_required(VERSION 3.14)
project(zswag)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ZSWAG_VERSION 0.7.0)

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

if (NOT TARGET wheel)
  set(Python3_FIND_STRATEGY LOCATION)
  set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/deps/python-cmake-wheel")
  include(python-wheel)
  set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
endif ()

if (NOT TARGET yaml-cpp)
  add_subdirectory(deps/yaml-cpp)
endif ()

if (NOT TARGET stx)
  add_subdirectory(deps/stx)
endif ()

if (NOT TARGET Catch2)
  add_subdirectory(deps/Catch2)
endif ()

if (NOT TARGET httplib)
  add_subdirectory(deps/cpp-httplib)
  target_compile_definitions(httplib
    INTERFACE
    CPPHTTPLIB_OPENSSL_SUPPORT)
  target_link_libraries(httplib INTERFACE OpenSSL::SSL)
endif ()

if (NOT TARGET keychain)
  add_subdirectory(deps/keychain)
endif ()

if (NOT TARGET pybind11)
  add_subdirectory(deps/pybind11)
endif ()

if (NOT TARGET ZserioCppRuntime)
  add_subdirectory(deps/zserio/compiler/extensions/cpp/runtime/src)
endif ()

if (NOT TARGET zsr)
  add_subdirectory(deps/zsr)
endif ()

##############
# libs

add_subdirectory(libs/httpcl)
add_subdirectory(libs/zswagcl)
add_subdirectory(libs/pyzswagcl)
add_subdirectory(libs/zswag/test)

##############
# zswag server wheel

add_custom_target(zswag-server-wheel
  WORKING_DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}
  COMMAND
    ${Python3_EXECUTABLE} setup.py ${ZSWAG_VERSION} bdist_wheel -d "${WHEEL_DEPLOY_DIRECTORY}")
add_dependencies(wheel zswag-server-wheel)
