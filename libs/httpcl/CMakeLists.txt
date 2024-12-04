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

target_include_directories(httpcl
  PRIVATE
    src
    include/httpcl
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(httpcl
  PRIVATE
    stx
  PUBLIC
    zswag-common
    spdlog::spdlog
    httplib::httplib
    yaml-cpp)

if(ZSWAG_USE_CPP14)
  target_compile_features(httpcl PUBLIC cxx_std_14)
else()
  target_compile_features(httpcl PUBLIC cxx_std_17)
endif()

if (ZSWAG_KEYCHAIN_SUPPORT)
  target_compile_definitions(httpcl PRIVATE ZSWAG_KEYCHAIN_SUPPORT)
  target_link_libraries(httpcl PUBLIC keychain::keychain)
endif()

if(ZSWAG_ENABLE_TESTING)
  add_subdirectory(test)
endif()
