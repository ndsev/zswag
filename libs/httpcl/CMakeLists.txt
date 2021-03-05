cmake_minimum_required(VERSION 3.14.0)
project(httpcl)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(httpcl STATIC
  include/httpcl/http-client.hpp
  include/httpcl/http-settings.hpp
  include/httpcl/uri.hpp
  src/http-client.cpp
  src/http-settings.cpp
  src/uri.cpp)

target_link_libraries(httpcl
  PRIVATE
    stx
  PUBLIC
    keychain
    httplib::httplib
    yaml-cpp
  )

set_target_properties(httpcl PROPERTIES CXX_STANDARD 17)

target_include_directories(httpcl
  PRIVATE
    src
  include/httpcl
  PUBLIC
    include)

add_subdirectory(test)
