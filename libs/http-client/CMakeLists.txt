cmake_minimum_required(VERSION 3.14.0)
project(http-client)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(http-client STATIC
  src/http-client.cpp
  src/http-settings.cpp
  src/uri.cpp)

target_link_libraries(http-client
  PRIVATE
    stx
  PUBLIC
    keychain
    httplib::httplib
    yaml-cpp
  )

set_target_properties(http-client PROPERTIES CXX_STANDARD 17)

target_include_directories(http-client
  PRIVATE
    src
  include/http-client
  PUBLIC
    include)

add_subdirectory(test)
