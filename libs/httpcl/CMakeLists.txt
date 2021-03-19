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

target_compile_features(httpcl
  INTERFACE
    cxx_std_17)

target_link_libraries(httpcl
  PRIVATE
    stx
  PUBLIC
    keychain
    httplib::httplib
    yaml-cpp
  )

target_include_directories(httpcl
  PRIVATE
    src
  include/httpcl
  PUBLIC
    include)

add_subdirectory(test)
