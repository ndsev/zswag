cmake_minimum_required(VERSION 3.14.0)
project(httpcl)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

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
    yaml-cpp::yaml-cpp
  )

# Ensure httpcl depends on OpenSSL build to establish correct build order
add_dependencies(httpcl openssl_build)

# On Windows, make sure OpenSSL libraries are available before linking
if(WIN32)
    # Add dependencies on the actual OpenSSL targets to ensure proper build order
    add_dependencies(httpcl OpenSSL::SSL OpenSSL::Crypto)
endif()

target_include_directories(httpcl
  PRIVATE
    src
  include/httpcl
  PUBLIC
    include)

if (ZSWAG_KEYCHAIN_SUPPORT)
  target_compile_definitions(httpcl PRIVATE ZSWAG_KEYCHAIN_SUPPORT)
  target_link_libraries(httpcl PUBLIC keychain)
endif()

if(ZSWAG_ENABLE_TESTING)
  add_subdirectory(test)
endif()
