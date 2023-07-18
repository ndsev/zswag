# Ensure that CMake's FindModule impl. also consider the dependencies
# fetched/build by Conan
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

# TODO: Check that all expected options/variables are set before usage,
# this avoid unnecessary error investigation time

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
                "${CMAKE_BINARY_DIR}/conan.cmake"
                TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

# TODO Check if fetching the SQLite3 dependency is really needed (for all target platforms)
conan_cmake_configure(REQUIRES openssl/1.1.1f keychain/1.2.0
                    GENERATORS cmake_find_package
                    )
                    
if(isMultiConfig)
    message("Multi-config generator detected")
                        
    foreach(TYPE ${CMAKE_CONFIGURATION_TYPES})
        conan_cmake_autodetect(conan_settings BUILD_TYPE ${TYPE})

        set(ENV{CONAN_USER_HOME} "${CMAKE_BINARY_DIR}/.conan")

        if(ANDROID)

            set(CONAN_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/AndroidToolchainForConan.cmake")
            file(WRITE "${CONAN_TOOLCHAIN_FILE}"
                "set\(ANDROID_PLATFORM \"${ANDROID_NATIVE_API_LEVEL}\"\)\n"
                "set\(ANDROID_ABI \"${ANDROID_ABI}\"\)\n"
                "include\(\"${CMAKE_TOOLCHAIN_FILE}\"\)")
            list(APPEND conan_env_settings "CONAN_CMAKE_TOOLCHAIN_FILE=${CONAN_TOOLCHAIN_FILE}")

            if("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
                set(conan_arch "armv7")
                set(conan_compiler_prefix "armv7a-linux-androideabi")
            elseif("${ANDROID_ABI}" STREQUAL "arm64-v8a")
                set(conan_arch "armv8")
                set(conan_compiler_prefix "aarch64-linux-android")
            elseif("${ANDROID_ABI}" STREQUAL "x86")
                set(conan_arch "x86")
                set(conan_compiler_prefix "i686-linux-android")
            elseif("${ANDROID_ABI}" STREQUAL "x86_64")
                set(conan_arch "x86_64")
                set(conan_compiler_prefix "x86_64-linux-android")
            endif()

            list(APPEND conan_env_settings "CC=${ANDROID_TOOLCHAIN_ROOT}/bin/${conan_compiler_prefix}${ANDROID_NATIVE_API_LEVEL}-clang")
            list(APPEND conan_env_settings "CXX=${ANDROID_TOOLCHAIN_ROOT}/bin/${conan_compiler_prefix}${ANDROID_NATIVE_API_LEVEL}-clang++")

            set(ANDROID_PLATFORM ${ANDROID_NATIVE_API_LEVEL})
            include(${ANDROID_NDK}/build/cmake/android.toolchain.cmake)
            list(APPEND conan_settings "arch=${conan_arch}")
            list(APPEND conan_settings "os=Android")
            list(APPEND conan_settings "os.api_level=${ANDROID_NATIVE_API_LEVEL}")

        elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")

            # Assumption: Build with dockcross linux-armv7 (with GCC10 support)
            list(APPEND conan_env_settings "CONAN_CMAKE_TOOLCHAIN_FILE=$ENV{CMAKE_TOOLCHAIN_FILE}")
            # OpenSSL build with Conan uses a cross_compile prefix for the compiler
            # but this is not needed as the cc/cxx already contain the full/correct paths
            # -> set it to "" to avoid cmake configure error
            set(ENV{CROSS_COMPILE} "")
            # enforce 32bit by avoiding implicit '64bit' build assumption
            list(APPEND conan_settings "arch=armv7")

        elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64")

            # Assumption: MacOS M1
            list(APPEND conan_env_settings "CONAN_CMAKE_TOOLCHAIN_FILE=$ENV{CMAKE_TOOLCHAIN_FILE}")
            # OpenSSL build with Conan uses a cross_compile prefix for the compiler
            # but this is not needed as the cc/cxx already contain the full/correct paths
            # -> set it to "" to avoid cmake configure error
            set(ENV{CROSS_COMPILE} "")
            # enforce armv8 for arm64
            list(APPEND conan_settings "arch=armv8")

        endif()

        conan_cmake_install(PATH_OR_REFERENCE .
                        BUILD missing
                        REMOTE conancenter
                        SETTINGS_HOST ${conan_settings}
                        ENV_HOST ${conan_env_settings}
                        )
    endforeach()
else()
    message("Single config generator detected")
    conan_cmake_autodetect(conan_settings)

    set(ENV{CONAN_USER_HOME} "${CMAKE_BINARY_DIR}/.conan")

    if(ANDROID)

        set(CONAN_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/AndroidToolchainForConan.cmake")
        file(WRITE "${CONAN_TOOLCHAIN_FILE}"
            "set\(ANDROID_PLATFORM \"${ANDROID_NATIVE_API_LEVEL}\"\)\n"
            "set\(ANDROID_ABI \"${ANDROID_ABI}\"\)\n"
            "include\(\"${CMAKE_TOOLCHAIN_FILE}\"\)")
        list(APPEND conan_env_settings "CONAN_CMAKE_TOOLCHAIN_FILE=${CONAN_TOOLCHAIN_FILE}")

        if("${ANDROID_ABI}" STREQUAL "armeabi-v7a")
            set(conan_arch "armv7")
            set(conan_compiler_prefix "armv7a-linux-androideabi")
        elseif("${ANDROID_ABI}" STREQUAL "arm64-v8a")
            set(conan_arch "armv8")
            set(conan_compiler_prefix "aarch64-linux-android")
        elseif("${ANDROID_ABI}" STREQUAL "x86")
            set(conan_arch "x86")
            set(conan_compiler_prefix "i686-linux-android")
        elseif("${ANDROID_ABI}" STREQUAL "x86_64")
            set(conan_arch "x86_64")
            set(conan_compiler_prefix "x86_64-linux-android")
        endif()

        list(APPEND conan_env_settings "CC=${ANDROID_TOOLCHAIN_ROOT}/bin/${conan_compiler_prefix}${ANDROID_NATIVE_API_LEVEL}-clang")
        list(APPEND conan_env_settings "CXX=${ANDROID_TOOLCHAIN_ROOT}/bin/${conan_compiler_prefix}${ANDROID_NATIVE_API_LEVEL}-clang++")

        set(ANDROID_PLATFORM ${ANDROID_NATIVE_API_LEVEL})
        include(${ANDROID_NDK}/build/cmake/android.toolchain.cmake)
        list(APPEND conan_settings "arch=${conan_arch}")
        list(APPEND conan_settings "os=Android")
        list(APPEND conan_settings "os.api_level=${ANDROID_NATIVE_API_LEVEL}")

    elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm")

        # Assumption: Build with dockcross linux-armv7 (with GCC10 support)
        list(APPEND conan_env_settings "CONAN_CMAKE_TOOLCHAIN_FILE=$ENV{CMAKE_TOOLCHAIN_FILE}")
        # OpenSSL build with Conan uses a cross_compile prefix for the compiler
        # but this is not needed as the cc/cxx already contain the full/correct paths
        # -> set it to "" to avoid cmake configure error
        set(ENV{CROSS_COMPILE} "")
        # enforce 32bit by avoiding implicit '64bit' build assumption
        list(APPEND conan_settings "arch=armv7")

    elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64")

        # Assumption: MacOS M1
        list(APPEND conan_env_settings "CONAN_CMAKE_TOOLCHAIN_FILE=$ENV{CMAKE_TOOLCHAIN_FILE}")
        # OpenSSL build with Conan uses a cross_compile prefix for the compiler
        # but this is not needed as the cc/cxx already contain the full/correct paths
        # -> set it to "" to avoid cmake configure error
        set(ENV{CROSS_COMPILE} "")
        # enforce armv8 for arm64
        list(APPEND conan_settings "arch=armv8")

    endif()

    conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS_HOST ${conan_settings}
                    ENV_HOST ${conan_env_settings}
                    )


    endif()