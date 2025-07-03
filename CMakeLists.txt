
cmake_minimum_required(VERSION 3.22.3)

project(zswag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow version override for development builds.
# This enables PEP 440 compliant dev versions (e.g., 1.7.3.dev14) from setuptools_scm
# without requiring CMakeLists.txt updates between releases.
# For official releases, this value should match the git tag.
if(NOT DEFINED ZSWAG_VERSION)
  set(ZSWAG_VERSION 1.8.0)
endif()

option(ZSWAG_BUILD_WHEELS "Enable zswag whl-output to WHEEL_DEPLOY_DIRECTORY." ON)
option(ZSWAG_KEYCHAIN_SUPPORT "Enable zswag keychain support." ON)
option(ZSWAG_ENABLE_TESTING "Enable testing for the project" OFF)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  message (STATUS "Testing will be enabled as zswag is the top-level project.")
  set (ZSWAG_ENABLE_TESTING ON CACHE BOOL "By default, enable testing if this is the main project")
endif()

if (ZSWAG_ENABLE_TESTING)
  enable_testing()
endif()

if (NOT MSVC)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_CXX_FLAGS -fPIC)
else()
  # Enable exception handling and suppress C4530 warnings on MSVC
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

if (NOT ZSWAG_DEPLOY_DIR)
  set (ZSWAG_DEPLOY_DIR "${CMAKE_BINARY_DIR}/bin")
endif()
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ZSWAG_DEPLOY_DIR}")

##############
# Third-party dependencies via FetchContent
include(cmake/Dependencies.cmake)

# Determine platform-specific OpenSSL configuration
# Get number of processor cores for parallel builds
include(ProcessorCount)
ProcessorCount(N)

# Set OpenSSL install directory
set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/deps/openssl-install)

# Configure OpenSSL build
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_CONFIGURE_COMMAND perl ${openssl_SOURCE_DIR}/Configure VC-WIN64A --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
    else()
        set(OPENSSL_CONFIGURE_COMMAND perl ${openssl_SOURCE_DIR}/Configure VC-WIN32 --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
    endif()
    set(OPENSSL_BUILD_COMMAND nmake)
    set(OPENSSL_INSTALL_COMMAND nmake install_sw)
    set(OPENSSL_LIB_PREFIX "")
    set(OPENSSL_LIB_SUFFIX ".lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(OPENSSL_CONFIGURE_COMMAND ${openssl_SOURCE_DIR}/Configure darwin64-arm64-cc)
    else()
        set(OPENSSL_CONFIGURE_COMMAND ${openssl_SOURCE_DIR}/Configure darwin64-x86_64-cc)
    endif()
    if(NOT N EQUAL 0)
        set(OPENSSL_BUILD_COMMAND make -j${N})
        set(OPENSSL_INSTALL_COMMAND make -j${N} install_sw)
    else()
        set(OPENSSL_BUILD_COMMAND make)
        set(OPENSSL_INSTALL_COMMAND make install_sw)
    endif()
    set(OPENSSL_LIB_PREFIX "lib")
    set(OPENSSL_LIB_SUFFIX ".a")
else() # Linux and other Unix-like systems
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_CONFIGURE_COMMAND ${openssl_SOURCE_DIR}/Configure linux-x86_64)
    else()
        set(OPENSSL_CONFIGURE_COMMAND ${openssl_SOURCE_DIR}/Configure linux-x86)
    endif()
    if(NOT N EQUAL 0)
        set(OPENSSL_BUILD_COMMAND make -j${N})
        set(OPENSSL_INSTALL_COMMAND make -j${N} install_sw)
    else()
        set(OPENSSL_BUILD_COMMAND make)
        set(OPENSSL_INSTALL_COMMAND make install_sw)
    endif()
    set(OPENSSL_LIB_PREFIX "lib")
    set(OPENSSL_LIB_SUFFIX ".a")
endif()

# Build OpenSSL using FetchContent
if(NOT TARGET openssl)
    add_subdirectory(${openssl_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/openssl_build EXCLUDE_FROM_ALL)
    set(OPENSSL_SOURCE_DIR ${openssl_SOURCE_DIR})

    add_custom_command(
        OUTPUT ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
               ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OPENSSL_INSTALL_DIR}
        COMMAND ${OPENSSL_CONFIGURE_COMMAND}
        COMMAND ${OPENSSL_BUILD_COMMAND}
        COMMAND ${OPENSSL_INSTALL_COMMAND}
        DEPENDS openssl
        COMMENT "Building and installing OpenSSL"
        VERBATIM
    )

    add_custom_target(openssl_build
        ALL
        DEPENDS ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
                ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
    )
endif()

# Create imported targets for OpenSSL::SSL and OpenSSL::Crypto
add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)

# Set target properties - use standard lib path
set_target_properties(OpenSSL::SSL PROPERTIES
    IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
)

set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
)

# Add a post-build command to handle lib64 vs lib directory issue on Unix systems
# This ensures libraries are available in the expected lib/ directory
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(
        TARGET openssl_build POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OPENSSL_INSTALL_DIR}/lib
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENSSL_INSTALL_DIR}/lib64/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
            ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENSSL_INSTALL_DIR}/lib64/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
            ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
        COMMENT "Ensuring OpenSSL libraries are available in lib/ directory"
        VERBATIM
    )
endif()

# Add dependencies
add_dependencies(OpenSSL::SSL openssl_build)
add_dependencies(OpenSSL::Crypto openssl_build)

# Link SSL to Crypto
set_target_properties(OpenSSL::SSL PROPERTIES
    INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
)

# Platform-specific system libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(OpenSSL::Crypto PROPERTIES
        INTERFACE_LINK_LIBRARIES "ws2_32;gdi32;advapi32;crypt32;user32"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(OpenSSL::Crypto PROPERTIES
        INTERFACE_LINK_LIBRARIES "dl;pthread"
    )
endif()

# zlib
if(NOT TARGET ZLIB::ZLIB)
    add_subdirectory(${zlib_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/zlib_build EXCLUDE_FROM_ALL)
    add_library(ZLIB::ZLIB ALIAS zlib)
endif()

# spdlog
if(NOT TARGET spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/spdlog_build EXCLUDE_FROM_ALL)
endif()

# yaml-cpp
if(NOT TARGET yaml-cpp)
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    add_subdirectory(${yaml_cpp_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp_build EXCLUDE_FROM_ALL)
endif()

# stx
if(NOT TARGET stx)
    add_subdirectory(${stx_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/stx_build EXCLUDE_FROM_ALL)
endif()

# speedyj
if(NOT TARGET speedyj)
    add_subdirectory(${speedyj_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/speedyj_build EXCLUDE_FROM_ALL)
endif()

# Catch2 (for testing)
if(ZSWAG_ENABLE_TESTING AND NOT TARGET Catch2::Catch2)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${catch2_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/catch2_build EXCLUDE_FROM_ALL)
endif()

# httplib
if(NOT TARGET httplib)
    set(HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN OFF CACHE BOOL "" FORCE)
    add_subdirectory(${httplib_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/httplib_build EXCLUDE_FROM_ALL)
    # Configure httplib with OpenSSL and zlib support
    target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
    # Ensure httplib depends on OpenSSL build to establish correct build order
    add_dependencies(httplib openssl_build)
endif()

# keychain
if (ZSWAG_KEYCHAIN_SUPPORT AND NOT TARGET keychain)
    add_subdirectory(${keychain_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/keychain_build EXCLUDE_FROM_ALL)
endif()

# pybind11
if(NOT TARGET pybind11)
    add_subdirectory(${pybind11_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/pybind11_build EXCLUDE_FROM_ALL)
endif()

# python-cmake-wheel
if (ZSWAG_BUILD_WHEELS AND NOT TARGET wheel)
    set(CMAKE_MODULE_PATH "${python_cmake_wheel_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
    include(python-wheel)
    set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")
endif()

# zserio-cmake-helper
if(NOT TARGET ZserioCppRuntime)
    set(ZSERIO_VERSION "2.13.0")
    add_subdirectory(${zserio_cmake_helper_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/zserio-cmake-helper_build EXCLUDE_FROM_ALL)
    add_zserio_cpp_runtime()
endif()

##############
# libs

add_subdirectory(libs/httpcl)
add_subdirectory(libs/zswagcl)

##############
# wheels

if (ZSWAG_BUILD_WHEELS)
  add_subdirectory(libs/pyzswagcl)
  add_subdirectory(libs/zswag/test)

  add_custom_target(zswag-server-wheel
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_LIST_DIR}
    COMMAND
      ${Python3_EXECUTABLE} setup.py ${ZSWAG_VERSION} bdist_wheel -d "${WHEEL_DEPLOY_DIRECTORY}")
  add_dependencies(wheel zswag-server-wheel)
endif()

