# Dependencies.cmake
# Centralized dependency management using FetchContent

include(FetchContent)

# Option to enable offline builds
option(FETCHCONTENT_FULLY_DISCONNECTED "Enable offline build mode" OFF)

# Set FetchContent properties
set(FETCHCONTENT_QUIET OFF)

# =============================================================================
# Local override checks and messages
# =============================================================================

macro(check_fetchcontent_override dep_name)
    if(FETCHCONTENT_SOURCE_DIR_${dep_name})
        message(STATUS "Using local source for ${dep_name}: ${FETCHCONTENT_SOURCE_DIR_${dep_name}}")
    endif()
endmacro()

# =============================================================================
# Core dependencies
# =============================================================================

# zlib
if(NOT TARGET zlib AND NOT TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    check_fetchcontent_override(ZLIB)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        "v1.3.1"
        GIT_SHALLOW    TRUE
    )
    set(ZLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(zlib)
endif()

# create ZLIB::ZLIB alias if it doesn't exist
if(NOT TARGET ZLIB::ZLIB)
    if(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
    elseif(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
endif()

# spdlog
if(NOT TARGET spdlog)
    check_fetchcontent_override(SPDLOG)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        "v1.15.3"
        GIT_SHALLOW    TRUE
    )
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

# yaml-cpp - use specific commit hash for stability
if(NOT TARGET yaml-cpp)
    check_fetchcontent_override(YAML_CPP)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG        "2f86d13775d119edbb69af52e5f566fd65c6953b"  # Latest commit on master as of Dec 2024
        GIT_SHALLOW    TRUE
    )
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(yaml-cpp)
endif()

# stx - use specific commit hash for stability
if(NOT TARGET stx)
    check_fetchcontent_override(STX)
    FetchContent_Declare(
        stx
        GIT_REPOSITORY https://github.com/Klebert-Engineering/stx.git
        GIT_TAG        "019d7e4978a7e60ac90223a96ad87963840308ef"  # Latest commit on main as of Dec 2024
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(stx)
endif()

# httplib - configure to not install/export targets to avoid dependency issues
if(NOT TARGET httplib)
    check_fetchcontent_override(HTTPLIB)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        "v0.15.3"
        GIT_SHALLOW    TRUE
    )
    set(HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)  # Disable installation to avoid export issues
    FetchContent_MakeAvailable(httplib)
endif()

# OpenSSL
if(NOT TARGET OpenSSL::SSL AND NOT TARGET OpenSSL::Crypto)
    check_fetchcontent_override(OPENSSL)
    FetchContent_Declare(
        openssl
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG        "openssl-3.5.1"
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(openssl)
    set(OPENSSL_SOURCE_DIR ${openssl_SOURCE_DIR})
    # Include the OpenSSL build logic
    include(${CMAKE_CURRENT_LIST_DIR}/OpenSSL.cmake)
endif()

# pybind11 (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS AND NOT TARGET pybind11)
    check_fetchcontent_override(PYBIND11)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        "v2.10.4"
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(pybind11)
endif()

# python-cmake-wheel (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS AND NOT TARGET wheel)
    check_fetchcontent_override(PYTHON_CMAKE_WHEEL)
    FetchContent_Declare(
        python-cmake-wheel
        GIT_REPOSITORY https://github.com/Klebert-Engineering/python-cmake-wheel.git
        GIT_TAG        "v1.0.0"
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(python-cmake-wheel)
endif()

# keychain
if(ZSWAG_KEYCHAIN_SUPPORT AND NOT TARGET keychain)
    check_fetchcontent_override(KEYCHAIN)
    FetchContent_Declare(
        keychain
        GIT_REPOSITORY https://github.com/hrantzsch/keychain
        GIT_TAG        "v1.3.1"
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(keychain)
endif()

# Catch2 (for testing)
if(ZSWAG_ENABLE_TESTING AND NOT TARGET Catch2::Catch2)
    check_fetchcontent_override(CATCH2)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        "v3.8.1"
        GIT_SHALLOW    TRUE
    )
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(catch2)
endif()

# zserio-cmake-helper
if(NOT COMMAND add_zserio_cpp_runtime)
    check_fetchcontent_override(ZSERIO_CMAKE_HELPER)
    FetchContent_Declare(
        zserio-cmake-helper
        GIT_REPOSITORY https://github.com/Klebert-Engineering/zserio-cmake-helper.git
        GIT_TAG        "v1.1.4"
        GIT_SHALLOW    TRUE
    )
    set(ZSERIO_VERSION "2.16.1")
    FetchContent_MakeAvailable(zserio-cmake-helper)

    # Add zserio C++ runtime target
    if(NOT TARGET ZserioCppRuntime)
        add_zserio_cpp_runtime()
    endif() 
endif()

# =============================================================================
# Configure httplib with OpenSSL and zlib support
# =============================================================================

if(TARGET httplib)
    target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
    # Ensure httplib depends on OpenSSL build to establish correct build order
    if(TARGET openssl_build)
        add_dependencies(httplib openssl_build)
    endif()
endif()

# =============================================================================
# Configure python-cmake-wheel integration
# =============================================================================

if(ZSWAG_BUILD_WHEELS)
    set(CMAKE_MODULE_PATH "${python-cmake-wheel_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
    
    if(NOT TARGET wheel)
        set(Python3_FIND_STRATEGY LOCATION)
        include(python-wheel)
        set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
    endif()
endif()
