cmake_minimum_required(VERSION 3.14.0)
project(zswagcl)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(zswagcl SHARED
  src/base64.cpp
  #src/http-service.cpp
  src/openapi-client.cpp
  src/openapi-config.cpp
  src/openapi-parameter-helper.cpp
  src/openapi-parser.cpp)

target_link_libraries(zswagcl
  PUBLIC
    stx
    httpcl
    zsr
    httplib::httplib
    yaml-cpp
    ZserioCppRuntime)

target_compile_features(zswagcl
  PUBLIC
    cxx_std_17)

target_include_directories(zswagcl
  PRIVATE
    src
    include/zswagcl
  PUBLIC
    include)

add_subdirectory(test)
