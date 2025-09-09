# Dependencies.cmake
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
    URI "gh:fmtlib/fmt#11.1.3"
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
    URI "gh:jbeder/yaml-cpp#aa8d4e4750ec9fe9f8cc680eb90f1b15955c817e"
    OPTIONS
        "YAML_CPP_BUILD_TESTS OFF"
        "YAML_CPP_BUILD_TOOLS OFF"
        "YAML_CPP_BUILD_CONTRIB OFF"
)

# stx
CPMAddPackage("gh:Klebert-Engineering/stx@1.0.0")

# httplib
CPMAddPackage(
    URI "gh:yhirose/cpp-httplib@0.15.3"
    OPTIONS
        "HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN OFF"
        "HTTPLIB_INSTALL OFF"
)

# OpenSSL
set (OPENSSL_VERSION openssl-3.5.2)
CPMAddPackage("gh:klebert-engineering/openssl-cmake@1.0.0")

# pybind11 (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    CPMAddPackage("gh:pybind/pybind11@2.13.6")
endif()

# python-cmake-wheel (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    CPMAddPackage("gh:Klebert-Engineering/python-cmake-wheel@1.0.1")
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
# Configure httplib with OpenSSL and zlib support
# =============================================================================

if (TARGET httplib)
    target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
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