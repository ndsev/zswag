cmake_minimum_required(VERSION 3.14.0)
project(zswagcl)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(zswagcl SHARED
  src/base64.cpp
  src/http-service.cpp
  src/openapi-parser.cpp)

target_link_libraries(zswagcl
  PRIVATE
    stx
  PUBLIC
    httpcl
    zsr
    httplib::httplib
    yaml-cpp
    ZserioCppRuntime)

set_target_properties(zswagcl PROPERTIES CXX_STANDARD 17)

target_include_directories(zswagcl
  PRIVATE
    src
    include/zswagcl
  PUBLIC
    include)

add_subdirectory(test)
