cmake_minimum_required(VERSION 3.14.0)
project(zsr-service-client)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(zsr-service-client SHARED
  src/base64.cpp
  src/http-service.cpp
  src/openapi-parser.cpp)

target_link_libraries(zsr-service-client
  PRIVATE
    stx
  PUBLIC
    http-client
    zsr
    httplib::httplib
    yaml-cpp
    ZserioCppRuntime)

set_target_properties(zsr-service-client PROPERTIES CXX_STANDARD 17)

target_include_directories(zsr-service-client
  PRIVATE
    src
    include/zsr-service-client
  PUBLIC
    include)

add_subdirectory(test)
