cmake_minimum_required(VERSION 3.14.0)
project(zswagcl)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(zswagcl SHARED
  src/base64.hpp
  include/zswagcl/private/openapi-client.hpp
  include/zswagcl/private/openapi-config.hpp
  include/zswagcl/private/openapi-parameter-helper.hpp
  include/zswagcl/private/openapi-parser.hpp
  include/zswagcl/oaclient.hpp

  src/base64.cpp
  src/openapi-client.cpp
  src/openapi-config.cpp
  src/openapi-parameter-helper.cpp
  src/openapi-parser.cpp
  src/oaclient.cpp)

target_link_libraries(zswagcl
  PUBLIC
    stx
    httpcl
    httplib::httplib
    yaml-cpp::yaml-cpp
    ZserioCppRuntime)

# Ensure zswagcl depends on OpenSSL build to establish correct build order
add_dependencies(zswagcl openssl_build)

# On Windows, make sure OpenSSL libraries are available before linking
if(WIN32)
    # Add dependencies on the actual OpenSSL targets to ensure proper build order
    add_dependencies(zswagcl OpenSSL::SSL OpenSSL::Crypto)
endif()

target_compile_definitions(zswagcl
  PRIVATE
    -DZSWAGCL_BUILD=1)

target_compile_features(zswagcl
  PUBLIC
    cxx_std_17)

target_include_directories(zswagcl
  PRIVATE
    src
    include/zswagcl
  PUBLIC
    include)
if(ZSWAG_ENABLE_TESTING)
  add_subdirectory(test)
endif()
