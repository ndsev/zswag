cmake_minimum_required(VERSION 3.22.3) # to build without conan
include(ExternalProject)
project(zswag)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-pedantic-errors)
set(ZSWAG_VERSION 1.6.7)

option(ZSWAG_KEYCHAIN_SUPPORT "Enable zswag keychain support." OFF)
option(ZSWAG_ENABLE_TESTING "Enable testing for the project" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set (ZSWAG_ENABLE_TESTING ON CACHE BOOL "By default, enable testing if this is the main project")
endif()

if (ZSWAG_ENABLE_TESTING)
  enable_testing()
endif()

if (NOT MSVC)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_FLAGS -fPIC)
endif()

if (NOT ZSWAG_DEPLOY_DIR)
  set (ZSWAG_DEPLOY_DIR "${CMAKE_BINARY_DIR}/bin")
endif()
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")

##############
# Build all dependencies from source

## OpenSSL
if(NOT TARGET OpenSSL)
  if (APPLE)
    set(OPENSSL_LIBRARY_SUFFIX "dylib")
  elseif (UNIX)
    set(OPENSSL_LIBRARY_SUFFIX "so")
  elseif(WIN32)
    set(OPENSSL_LIBRARY_SUFFIX "dll")
  endif()

  set(OPENSSL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/openssl)
  set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl)
  set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include)
  set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/config)

  ExternalProject_Add(
  OpenSSL
  SOURCE_DIR ${OPENSSL_SOURCE_DIR}
  CONFIGURE_COMMAND
    ${OPENSSL_CONFIGURE_COMMAND}
    --prefix=${OPENSSL_INSTALL_DIR}
    --openssldir=${OPENSSL_INSTALL_DIR}
    --libdir=${OPENSSL_INSTALL_DIR}/lib
  BUILD_COMMAND make -j 8
  TEST_COMMAND ""
  UPDATE_COMMAND ""
  INSTALL_COMMAND make install_sw
  INSTALL_DIR ${OPENSSL_INSTALL_DIR}
  )

  # We cannot use find_library because ExternalProject_Add() is performed at build time.
  # And to please the property INTERFACE_INCLUDE_DIRECTORIES,
  # we make the include directory in advance.
  file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

  add_library(OpenSSL::Crypto SHARED IMPORTED GLOBAL)
  set_property(TARGET OpenSSL::Crypto PROPERTY IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/libcrypto.${OPENSSL_LIBRARY_SUFFIX})
  set_property(TARGET OpenSSL::Crypto PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
  add_dependencies(OpenSSL::Crypto OpenSSL)

  add_library(OpenSSL::SSL SHARED IMPORTED GLOBAL)
  set_property(TARGET OpenSSL::SSL PROPERTY IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/libssl.${OPENSSL_LIBRARY_SUFFIX})
  set_property(TARGET OpenSSL::SSL PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR})
  add_dependencies(OpenSSL::SSL OpenSSL)
endif()

## keychain
if(ZSWAG_KEYCHAIN_SUPPORT)
  add_subdirectory(deps/keychain)
endif()

## yaml-cpp
if (NOT TARGET yaml-cpp)
  add_subdirectory(deps/yaml-cpp)
endif()

## stx
if (NOT TARGET stx)
  add_subdirectory(deps/stx)
endif()

## speedyj
if (NOT TARGET speedyj)
  add_subdirectory(deps/speedyj)
endif()

## Catch2
if (NOT TARGET Catch2)
  add_subdirectory(deps/catch2)
endif()

## httplib
if (NOT TARGET httplib)
  add_subdirectory(deps/cpp-httplib)
  target_compile_definitions(httplib
    INTERFACE
    CPPHTTPLIB_OPENSSL_SUPPORT)
  target_link_libraries(httplib INTERFACE OpenSSL::SSL)
endif()

## spdlog
if (NOT TARGET spdlog)
  add_subdirectory(deps/spdlog)
endif()

## zserio cmake helper
if (NOT TARGET zserio-cmake-helper)
  set(ZSERIO_VERSION "2.13.0")
  add_subdirectory(deps/zserio-cmake-helper)
  if (NOT TARGET ZserioCppRuntime)
    add_zserio_cpp_runtime()
  endif()
endif()

##############
# libs

add_subdirectory(libs/httpcl)
add_subdirectory(libs/zswagcl)

##############
# deploy openssl libs

if (WIN32)
  set(OPENSSL_DEPLOY_DIR "${ZSWAG_DEPLOY_DIR}/${CMAKE_BUILD_TYPE}")
else()
  set(OPENSSL_DEPLOY_DIR "${ZSWAG_DEPLOY_DIR}")
endif()

message(STATUS "Deploying to ${OPENSSL_DEPLOY_DIR}")
message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")

if(APPLE)
  set(OPENSSL_LIB_DIR "${OPENSSL_INCLUDE_DIR}/../lib")
  set(OPENSSL_LIBS
      "${OPENSSL_LIB_DIR}/libcrypto.3.dylib"
      "${OPENSSL_LIB_DIR}/libssl.3.dylib"
  )
elseif(MSVC)
  set(OPENSSL_LIB_DIR "${OPENSSL_INCLUDE_DIR}/../bin")
  set(OPENSSL_LIBS
      "${OPENSSL_LIB_DIR}/libcrypto-3-x64.dll"
      "${OPENSSL_LIB_DIR}/libssl-3-x64.dll"
  )
elseif(UNIX AND NOT APPLE)
  set(OPENSSL_LIB_DIR "${OPENSSL_INCLUDE_DIR}/../lib")
  set(OPENSSL_LIBS
      "${OPENSSL_LIB_DIR}/libcrypto.so.3"
      "${OPENSSL_LIB_DIR}/libssl.so.3"
  )
endif()

foreach(file_i ${OPENSSL_LIBS})
    get_filename_component(filename ${file_i} NAME)
    add_custom_command(
        OUTPUT "${OPENSSL_DEPLOY_DIR}/${filename}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${file_i}"
            "${OPENSSL_DEPLOY_DIR}/${filename}"
        DEPENDS "${file_i}"
        COMMENT "Copying ${file_i} to ${OPENSSL_DEPLOY_DIR}"
    )
    list(APPEND COPIED_OPENSSL_LIBS "${OPENSSL_DEPLOY_DIR}/${filename}")
endforeach(file_i)

add_custom_target(copy_openssl_libs ALL DEPENDS ${COPIED_OPENSSL_LIBS})

add_dependencies(zswagcl copy_openssl_libs)
add_dependencies(zswagcl OpenSSL::SSL OpenSSL::Crypto)
add_dependencies(copy_openssl_libs OpenSSL::SSL OpenSSL::Crypto)





