# deps.cmake
# Centralized dependency management using CPM (CMake Package Manager)
# CPM provides better version management and diagnostics compared to raw FetchContent

# =============================================================================
# Core dependencies
# =============================================================================

# zlib
CPMAddPackage(
    URI "gh:madler/zlib@1.3.1"
    OPTIONS
        "ZLIB_BUILD_EXAMPLES OFF"
        "BUILD_TESTING OFF")
if(zlib_ADDED)
    set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(zlibstatic PROPERTIES EXCLUDE_FROM_ALL TRUE)
    # Create ZLIB::ZLIB alias if it doesn't exist
    if(NOT TARGET ZLIB::ZLIB)
        if(TARGET zlib)
            add_library(ZLIB::ZLIB ALIAS zlib)
        elseif(TARGET zlibstatic)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        endif()
    endif()
endif()

# fmt
CPMAddPackage(
    URI "gh:fmtlib/fmt#11.1.4"
    OPTIONS "FMT_HEADER_ONLY OFF"
)

# spdlog
CPMAddPackage(
    URI "gh:gabime/spdlog@1.15.3"
    OPTIONS
        "SPDLOG_BUILD_TESTS OFF"
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_FMT_EXTERNAL ON"
)

# yaml-cpp
CPMAddPackage(
    NAME yaml-cpp
    GITHUB_REPOSITORY jbeder/yaml-cpp
    VERSION 0.9.0
    GIT_TAG yaml-cpp-0.9.0
    OPTIONS
        "YAML_CPP_BUILD_TESTS OFF"
        "YAML_CPP_BUILD_TOOLS OFF"
        "YAML_CPP_BUILD_CONTRIB OFF"
)

# stx
CPMAddPackage("gh:Klebert-Engineering/stx@1.0.0")

# OpenSSL
set (OPENSSL_VERSION openssl-3.5.2)
CPMAddPackage("gh:klebert-engineering/openssl-cmake@1.0.0")

# nghttp2
if(DEFINED BUILD_SHARED_LIBS)
    set(ZSWAG_BUILD_SHARED_LIBS_WAS_DEFINED ON)
    set(ZSWAG_BUILD_SHARED_LIBS_PREVIOUS "${BUILD_SHARED_LIBS}")
else()
    set(ZSWAG_BUILD_SHARED_LIBS_WAS_DEFINED OFF)
endif()
set(BUILD_SHARED_LIBS OFF)
CPMAddPackage(
    NAME nghttp2
    GITHUB_REPOSITORY nghttp2/nghttp2
    GIT_TAG v1.69.0
    OPTIONS
        "ENABLE_LIB_ONLY ON"
        "BUILD_STATIC_LIBS ON"
        "BUILD_SHARED_LIBS OFF"
        "BUILD_TESTING OFF")
if(nghttp2_ADDED AND TARGET nghttp2_static)
    set_target_properties(nghttp2_static PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

# libcurl
if(TARGET nghttp2_static)
    # curl's CMake module can consume an existing target name as the library.
    # The linked nghttp2 target carries the generated include directory needed
    # for nghttp2ver.h.
    set(NGHTTP2_INCLUDE_DIR "${nghttp2_SOURCE_DIR}/lib/includes" CACHE PATH "nghttp2 include directory" FORCE)
    set(NGHTTP2_LIBRARY nghttp2_static CACHE STRING "nghttp2 target" FORCE)
    set(NGHTTP2_USE_STATIC_LIBS ON CACHE BOOL "Use static nghttp2" FORCE)
endif()
if(TARGET OpenSSL::SSL)
    get_target_property(ZSWAG_OPENSSL_INCLUDE_DIR OpenSSL::SSL INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(ZSWAG_OPENSSL_SSL_LIBRARY OpenSSL::SSL IMPORTED_LOCATION)
    get_target_property(ZSWAG_OPENSSL_CRYPTO_LIBRARY OpenSSL::Crypto IMPORTED_LOCATION)
    set(OPENSSL_INCLUDE_DIR "${ZSWAG_OPENSSL_INCLUDE_DIR}" CACHE PATH "OpenSSL include directory" FORCE)
    set(OPENSSL_SSL_LIBRARY "${ZSWAG_OPENSSL_SSL_LIBRARY}" CACHE FILEPATH "OpenSSL SSL library" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${ZSWAG_OPENSSL_CRYPTO_LIBRARY}" CACHE FILEPATH "OpenSSL crypto library" FORCE)
    set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "Use static OpenSSL" FORCE)
endif()
CPMAddPackage(
    NAME CURL
    GITHUB_REPOSITORY curl/curl
    GIT_TAG curl-8_20_0
    OPTIONS
        "BUILD_CURL_EXE OFF"
        "BUILD_SHARED_LIBS OFF"
        "BUILD_STATIC_LIBS ON"
        "BUILD_EXAMPLES OFF"
        "BUILD_TESTING OFF"
        "BUILD_LIBCURL_DOCS OFF"
        "BUILD_MISC_DOCS OFF"
        "ENABLE_CURL_MANUAL OFF"
        "CURL_DISABLE_INSTALL ON"
        "CURL_USE_OPENSSL ON"
        "CURL_ZLIB OFF"
        "USE_NGHTTP2 ON"
        "CURL_USE_PKGCONFIG OFF"
        "CURL_USE_CMAKECONFIG OFF"
        "CURL_BROTLI OFF"
        "CURL_ZSTD OFF"
        "CURL_USE_LIBPSL OFF"
        "CURL_USE_LIBSSH2 OFF"
        "CURL_USE_LIBSSH OFF"
        "CURL_USE_GSASL OFF"
        "CURL_USE_GSSAPI OFF"
        "USE_LIBIDN2 OFF"
        "HTTP_ONLY ON")
if(TARGET libcurl_static)
    set_target_properties(libcurl_static PROPERTIES EXCLUDE_FROM_ALL TRUE)
    add_dependencies(libcurl_static openssl_build)
endif()
if(TARGET nghttp2_static AND NOT TARGET CURL::nghttp2)
    # curl's generated target can reference this dependency target even when
    # its bundled FindNGHTTP2 module resolved the library from cache variables.
    add_library(CURL::nghttp2 INTERFACE IMPORTED GLOBAL)
    set_target_properties(CURL::nghttp2 PROPERTIES
        INTERFACE_LINK_LIBRARIES nghttp2_static)
endif()
if(ZSWAG_BUILD_SHARED_LIBS_WAS_DEFINED)
    set(BUILD_SHARED_LIBS "${ZSWAG_BUILD_SHARED_LIBS_PREVIOUS}")
else()
    unset(BUILD_SHARED_LIBS)
endif()

# pybind11 (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    CPMAddPackage("gh:pybind/pybind11@2.13.6")
endif()

# python-cmake-wheel (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    CPMAddPackage("gh:Klebert-Engineering/python-cmake-wheel@1.2.8")
endif()

# keychain
if(ZSWAG_KEYCHAIN_SUPPORT)
    CPMAddPackage("gh:hrantzsch/keychain@1.3.1")
endif()

# Catch2 (for testing)
if(ZSWAG_ENABLE_TESTING)
    CPMAddPackage(
        URI "gh:catchorg/Catch2@3.8.1"
        OPTIONS
            "CATCH_INSTALL_DOCS OFF"
            "CATCH_INSTALL_EXTRAS OFF"
    )
endif()

# zserio-cmake-helper
set(ZSERIO_VERSION "2.16.1")
CPMAddPackage("gh:Klebert-Engineering/zserio-cmake-helper@1.1.4")
# Add zserio C++ runtime target
if(NOT TARGET ZserioCppRuntime)
    add_zserio_cpp_runtime()
endif()

# =============================================================================
# Configure python-cmake-wheel integration
# =============================================================================

if(ZSWAG_BUILD_WHEELS AND python-cmake-wheel_ADDED)
    set(CMAKE_MODULE_PATH "${python-cmake-wheel_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
    
    if(NOT TARGET wheel)
        set(Python3_FIND_STRATEGY LOCATION)
        include(python-wheel)
        set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
    endif()
endif()
