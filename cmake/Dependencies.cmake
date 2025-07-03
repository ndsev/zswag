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
    GIT_TAG        5a82f71ed1dfc0bec044d9702463dbdf84ea3b71
)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        27cb4c76708608465c413f6d0e6b8d99a4d84302
)

# yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        1d8ca1f35eb3a9c9142462b28282a848e5d29a91
)

# stx
FetchContent_Declare(
    stx
    GIT_REPOSITORY https://github.com/Klebert-Engineering/stx.git
    GIT_TAG        019d7e4978a7e60ac90223a96ad87963840308ef
)

# speedyj
FetchContent_Declare(
    speedyj
    GIT_REPOSITORY https://github.com/Klebert-Engineering/speedyj.git
    GIT_TAG        d7d2087085b45411118d74f4d7353357cb54540d
)

# httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        0b875e07471fa43f4612fd3355e218099ccb4a79
)

# OpenSSL
FetchContent_Declare(
    openssl
    GIT_REPOSITORY https://github.com/openssl/openssl.git
    GIT_TAG        f6ce48f5b8ad4d8d748ea87d2490cbed08db9936
)

# pybind11 (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        654fe92652e6dc0eec80b1877b531aaab3a3e56c
    )
endif()

# python-cmake-wheel (only needed when building wheels)
if(ZSWAG_BUILD_WHEELS)
    FetchContent_Declare(
        python-cmake-wheel
        GIT_REPOSITORY https://github.com/Klebert-Engineering/python-cmake-wheel.git
        GIT_TAG        3be3c0d71fd6fefcdfd5b1b0b9dbd00c2bd0cff8
    )
endif()

# zserio-cmake-helper
FetchContent_Declare(
    zserio-cmake-helper
    GIT_REPOSITORY https://github.com/Klebert-Engineering/zserio-cmake-helper.git
    GIT_TAG        caf0ddb439e02b52853c063d611dcd6aa2429ff1
)

# keychain
FetchContent_Declare(
    keychain
    GIT_REPOSITORY https://github.com/Klebert-Engineering/keychain.git
    GIT_TAG        ed1610d27f1788836ce8d26c147ea7bc0e4e85e7
)

# Catch2 (for testing)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        4e8d92bf02f7d1c8006a0e7a5ecabd8e62d98502
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

# Configure zlib
FetchContent_MakeAvailable(zlib)

# Create ZLIB::ZLIB alias if it doesn't exist
if(TARGET zlib AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlib)
endif()
if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# Configure spdlog
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

# Configure httplib
set(HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

# Configure pybind11 and python-cmake-wheel (only when building wheels)
if(ZSWAG_BUILD_WHEELS)
    FetchContent_MakeAvailable(pybind11)
    FetchContent_MakeAvailable(python-cmake-wheel)
endif()

# Configure zserio-cmake-helper
set(ZSERIO_VERSION "2.16.1")
FetchContent_MakeAvailable(zserio-cmake-helper)

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

if(NOT TARGET ZserioCppRuntime)
    add_zserio_cpp_runtime()
endif() 