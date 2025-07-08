# OpenSSL.cmake
# Custom OpenSSL build configuration using ExternalProject

include(ExternalProject)
include(ProcessorCount)

# Get number of processor cores for parallel builds
ProcessorCount(N)

# Set OpenSSL install directory
set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/openssl-install)

# Determine platform-specific OpenSSL configuration
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_CONFIGURE_COMMAND perl ${OPENSSL_SOURCE_DIR}/Configure VC-WIN64A --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
    else()
        set(OPENSSL_CONFIGURE_COMMAND perl ${OPENSSL_SOURCE_DIR}/Configure VC-WIN32 --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
    endif()
    set(OPENSSL_BUILD_COMMAND nmake)
    set(OPENSSL_INSTALL_COMMAND nmake install_sw)
    # On Windows, OpenSSL may generate libraries with lib prefix
    set(OPENSSL_LIB_PREFIX "lib")
    set(OPENSSL_LIB_SUFFIX ".lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/Configure darwin64-arm64-cc --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
    else()
        set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/Configure darwin64-x86_64-cc --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
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
    # Detect architecture for Linux
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/Configure linux-aarch64 --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/Configure linux-x86_64 --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
    else()
        set(OPENSSL_CONFIGURE_COMMAND ${OPENSSL_SOURCE_DIR}/Configure linux-x86 --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl no-shared no-tests)
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

# Determine the library directory used by OpenSSL on Linux
# x86_64 Linux uses lib64, but ARM64 uses lib
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(OPENSSL_LIBDIR "lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OPENSSL_LIBDIR "lib64")
else()
    set(OPENSSL_LIBDIR "lib")
endif()

# Build OpenSSL using ExternalProject
ExternalProject_Add(openssl_build
    SOURCE_DIR ${OPENSSL_SOURCE_DIR}
    CONFIGURE_COMMAND ${OPENSSL_CONFIGURE_COMMAND}
    BUILD_COMMAND ${OPENSSL_BUILD_COMMAND}
    INSTALL_COMMAND ${OPENSSL_INSTALL_COMMAND}
    BUILD_IN_SOURCE 1
    BUILD_BYPRODUCTS 
        ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
        ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>:${OPENSSL_INSTALL_DIR}/lib/ssl${OPENSSL_LIB_SUFFIX}>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Windows>:${OPENSSL_INSTALL_DIR}/lib/crypto${OPENSSL_LIB_SUFFIX}>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${OPENSSL_INSTALL_DIR}/${OPENSSL_LIBDIR}/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}>
        $<$<STREQUAL:${CMAKE_SYSTEM_NAME},Linux>:${OPENSSL_INSTALL_DIR}/${OPENSSL_LIBDIR}/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}>
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    LOG_OUTPUT_ON_FAILURE ON
)

# Create imported targets for OpenSSL::SSL and OpenSSL::Crypto
add_library(OpenSSL::SSL STATIC IMPORTED GLOBAL)
add_library(OpenSSL::Crypto STATIC IMPORTED GLOBAL)

# Set target properties - use standard lib path
# On Windows, try both naming conventions
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/ssl${OPENSSL_LIB_SUFFIX}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
    )
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/crypto${OPENSSL_LIB_SUFFIX}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
    )
else()
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
    )
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX}
        INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INSTALL_DIR}/include
    )
endif()

# Add a post-build command to handle lib64 vs lib directory issue on Linux x86_64 systems only
# This ensures libraries are available in the expected lib/ directory
# macOS and ARM64 Linux install directly to lib/, so this is only needed for x86_64 Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
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

# Add a post-build command to handle library naming issues on Windows
# OpenSSL may generate libraries with different names, so we create fallback copies
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_custom_command(
        TARGET openssl_build POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}ssl${OPENSSL_LIB_SUFFIX} 
            ${OPENSSL_INSTALL_DIR}/lib/ssl${OPENSSL_LIB_SUFFIX}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            ${OPENSSL_INSTALL_DIR}/lib/${OPENSSL_LIB_PREFIX}crypto${OPENSSL_LIB_SUFFIX} 
            ${OPENSSL_INSTALL_DIR}/lib/crypto${OPENSSL_LIB_SUFFIX}
        COMMENT "Ensuring OpenSSL libraries are available with expected names"
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