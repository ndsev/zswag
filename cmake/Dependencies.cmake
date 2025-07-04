# Dependencies.cmake
# Centralized dependency management using FetchContent

include(FetchContent)

# Option to enable offline builds
option(FETCHCONTENT_FULLY_DISCONNECTED "Enable offline build mode" OFF)

# Set FetchContent properties
set(FETCHCONTENT_QUIET OFF)

# =============================================================================
# Core dependencies
# =============================================================================

# zlib
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        "v1.3.1"
)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        "v1.15.3"
)

# yaml-cpp - use specific commit hash for stability
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        "2f86d13775d119edbb69af52e5f566fd65c6953b"  # Latest commit on master as of Dec 2024
)

# stx - use specific commit hash for stability
FetchContent_Declare(
    stx
    GIT_REPOSITORY https://github.com/Klebert-Engineering/stx.git
    GIT_TAG        "019d7e4978a7e60ac90223a96ad87963840308ef"  # Latest commit on main as of Dec 2024
)

# speedyj - use specific commit hash for stability
FetchContent_Declare(
    speedyj
    GIT_REPOSITORY https://github.com/Klebert-Engineering/speedyj.git
    GIT_TAG        "d7d2087085b45411118d74f4d7353357cb54540d"  # Latest commit on master as of Dec 2024
)

# httplib - configure to not install/export targets to avoid dependency issues
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        "v0.15.3"
)

# OpenSSL
FetchContent_Declare(
    openssl
    GIT_REPOSITORY https://github.com/openssl/openssl.git
    GIT_TAG        "openssl-3.5.1"
)

# pybind11 (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        "v2.10.4"
    )
endif()

# python-cmake-wheel (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    FetchContent_Declare(
        python-cmake-wheel
        GIT_REPOSITORY https://github.com/Klebert-Engineering/python-cmake-wheel.git
        GIT_TAG        "v0.9.0"
    )
endif()

# keychain
FetchContent_Declare(
    keychain
    GIT_REPOSITORY https://github.com/hrantzsch/keychain
    GIT_TAG        "v1.3.1"
)

# Catch2 (for testing)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        "v3.8.1"
)

# zserio-cmake-helper
FetchContent_Declare(
    zserio-cmake-helper
    GIT_REPOSITORY https://github.com/Klebert-Engineering/zserio-cmake-helper.git
    GIT_TAG        "v1.1.4"
)

# =============================================================================
# Local override checks and messages
# =============================================================================

macro(check_fetchcontent_override dep_name)
    if(FETCHCONTENT_SOURCE_DIR_${dep_name})
        message(STATUS "Using local source for ${dep_name}: ${FETCHCONTENT_SOURCE_DIR_${dep_name}}")
    endif()
endmacro()

check_fetchcontent_override(ZLIB)
check_fetchcontent_override(SPDLOG)
check_fetchcontent_override(YAML_CPP)
check_fetchcontent_override(STX)
check_fetchcontent_override(SPEEDYJ)
check_fetchcontent_override(HTTPLIB)
check_fetchcontent_override(OPENSSL)
if(ZSWAG_BUILD_WHEELS)
    check_fetchcontent_override(PYBIND11)
    check_fetchcontent_override(PYTHON_CMAKE_WHEEL)
endif()
check_fetchcontent_override(ZSERIO_CMAKE_HELPER)
check_fetchcontent_override(KEYCHAIN)
check_fetchcontent_override(CATCH2)

# =============================================================================
# Configure and make dependencies available
# =============================================================================

# Configure zlib first - it's needed by httplib
set(ZLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)

# Create ZLIB::ZLIB alias if it doesn't exist
if(TARGET zlib AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
elseif(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# Configure spdlog
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# Configure yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# Configure stx
FetchContent_MakeAvailable(stx)

# Configure speedyj
FetchContent_MakeAvailable(speedyj)

# Configure httplib with special settings to avoid target export issues
set(HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN OFF CACHE BOOL "" FORCE)
set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)  # Disable installation to avoid export issues
FetchContent_MakeAvailable(httplib)

# Configure pybind11 and python-cmake-wheel (only when building wheels)
if(ZSWAG_BUILD_WHEELS)
    FetchContent_MakeAvailable(pybind11)
    FetchContent_MakeAvailable(python-cmake-wheel)
endif()

# Configure keychain (conditional)
if(ZSWAG_KEYCHAIN_SUPPORT)
    FetchContent_MakeAvailable(keychain)
endif()

# Configure Catch2 (conditional)
if(ZSWAG_ENABLE_TESTING)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(catch2)
endif()

# =============================================================================
# Custom OpenSSL handling
# =============================================================================

# OpenSSL requires special handling due to its complex build process
# We'll use FetchContent to get the source but ExternalProject to build it

FetchContent_GetProperties(openssl)
if(NOT openssl_POPULATED)
    FetchContent_Populate(openssl)
    set(OPENSSL_SOURCE_DIR ${openssl_SOURCE_DIR})
endif()

# Include the OpenSSL build logic
include(${CMAKE_CURRENT_LIST_DIR}/OpenSSL.cmake)

# =============================================================================
# Configure httplib with OpenSSL and zlib support
# =============================================================================

if(TARGET httplib)
    target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
    # Ensure httplib depends on OpenSSL build to establish correct build order
    add_dependencies(httplib openssl_build)
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

# =============================================================================
# Configure zserio-cmake-helper
# =============================================================================

# Configure zserio-cmake-helper
set(ZSERIO_VERSION "2.16.1")
FetchContent_MakeAvailable(zserio-cmake-helper)

# Add zserio C++ runtime target
if(NOT TARGET ZserioCppRuntime)
    add_zserio_cpp_runtime()
endif() 