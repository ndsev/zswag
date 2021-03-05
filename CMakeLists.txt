cmake_minimum_required(VERSION 3.14)
project(zswag)

if (NOT MSVC)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_FLAGS -fPIC)
endif()

##############
# deps

find_package(OpenSSL REQUIRED)

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
  add_subdirectory(deps/zserio-cpp-introspection)
endif ()

##############
# libs

add_subdirectory(libs/zswagcl)
# add_subdirectory(libs/pyzswagcl)
add_subdirectory(libs/httpcl)
