cmake_minimum_required(VERSION 3.14.0)
project(zswagcl)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(zswagcl SHARED
  src/base64.hpp
  include/zswagcl/openapi-client.hpp
  include/zswagcl/openapi-config.hpp
  include/zswagcl/openapi-parameter-helper.hpp
  include/zswagcl/openapi-parser.hpp
  include/zswagcl/zsr-client.hpp

  src/base64.cpp
  src/base64.hpp
  src/openapi-client.cpp
  include/zswagcl/openapi-client.hpp
  src/openapi-config.cpp
  include/zswagcl/openapi-config.hpp
  src/openapi-parameter-helper.cpp
  include/zswagcl/openapi-parameter-helper.hpp
  src/openapi-parser.cpp
  include/zswagcl/openapi-parser.hpp
  src/zsr-client.cpp
  include/zswagcl/zsr-client.hpp)

target_link_libraries(zswagcl
  PUBLIC
    stx
    httpcl
    zsr
    httplib::httplib
    yaml-cpp
    ZserioCppRuntime)

target_compile_definitions(zswagcl
  PRIVATE
    -DZSWAG_BUILD=1)

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
