cmake_minimum_required(VERSION 3.14.0)
project(httpcl)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(NOT TARGET spdlog)
  find_package(spdlog CONFIG REQUIRED)
endif()

add_library(httpcl STATIC
  include/httpcl/http-client.hpp
  include/httpcl/http-settings.hpp
  include/httpcl/uri.hpp
  include/httpcl/log.hpp
  src/http-client.cpp
  src/http-settings.cpp
  src/uri.cpp
  src/log.cpp)

target_compile_features(httpcl
  INTERFACE
    cxx_std_17)

target_link_libraries(httpcl
  PRIVATE
    stx
  PUBLIC
    spdlog::spdlog
    httplib::httplib
    yaml-cpp
  )

target_include_directories(httpcl
  PRIVATE
    src
  include/httpcl
  PUBLIC
    include)

if (ZSWAG_KEYCHAIN_SUPPORT)
  target_compile_definitions(httpcl PRIVATE ZSWAG_KEYCHAIN_SUPPORT)
  target_link_libraries(httpcl PUBLIC keychain::keychain)
endif()

if(ZSWAG_ENABLE_TESTING)
  add_subdirectory(test)
endif()
