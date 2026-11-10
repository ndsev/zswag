cmake_minimum_required(VERSION 3.22.3)

project(openssl-msvc-discovery-test NONE)

if(NOT ZSWAG_SOURCE_DIR)
    message(FATAL_ERROR "ZSWAG_SOURCE_DIR is required")
endif()

set(_fake_root "${CMAKE_CURRENT_BINARY_DIR}/fake-openssl")
file(MAKE_DIRECTORY "${_fake_root}/include/openssl" "${_fake_root}/lib" "${_fake_root}/no-system-libs")
file(WRITE "${_fake_root}/include/openssl/ssl.h" "")
file(WRITE "${_fake_root}/include/openssl/opensslv.h"
    "#define OPENSSL_VERSION_NUMBER 0x30502000L\n#define OPENSSL_VERSION_STR \"3.5.2\"\n")
file(WRITE "${_fake_root}/lib/ssl.lib" "")
file(WRITE "${_fake_root}/lib/crypto.lib" "")

add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)
set_target_properties(OpenSSL::SSL PROPERTIES
    IMPORTED_LOCATION "${_fake_root}/lib/ssl.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${_fake_root}/include")
set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION "${_fake_root}/lib/crypto.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${_fake_root}/include")

# Exercise FindOpenSSL's MSVC-specific variable path on every test platform.
set(WIN32 TRUE)
set(MSVC TRUE)
set(CMAKE_SIZEOF_VOID_P 8)
set(CMAKE_FIND_ROOT_PATH "${_fake_root}/no-system-libs")
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

include("${ZSWAG_SOURCE_DIR}/cmake/OpenSSLDiscovery.cmake")
zswag_set_openssl_discovery_variables()
find_package(OpenSSL REQUIRED)

if(NOT OPENSSL_SSL_LIBRARY STREQUAL "${_fake_root}/lib/ssl.lib")
    message(FATAL_ERROR "FindOpenSSL did not retain the bundled SSL library: ${OPENSSL_SSL_LIBRARY}")
endif()
if(NOT OPENSSL_CRYPTO_LIBRARY STREQUAL "${_fake_root}/lib/crypto.lib")
    message(FATAL_ERROR "FindOpenSSL did not retain the bundled crypto library: ${OPENSSL_CRYPTO_LIBRARY}")
endif()
