# coverage.cmake
# Code coverage configuration for zswag C++ libraries
#
# Usage:
#   Include this file in your CMakeLists.txt after enabling testing
#   Set ZSWAG_ENABLE_COVERAGE=ON to enable coverage
#
# Requirements:
#   - GCC or Clang compiler
#   - lcov (for generating reports)
#   - genhtml (for HTML reports)
#
# Targets created:
#   - coverage-clean: Clean coverage data
#   - coverage-report: Generate coverage report
#   - coverage: Run tests and generate coverage report

# Only enable coverage for Debug or RelWithDebInfo builds
if(ZSWAG_ENABLE_COVERAGE)
    if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
        message(WARNING "Coverage requires Debug or RelWithDebInfo build type. Current: ${CMAKE_BUILD_TYPE}")
        message(WARNING "Coverage will be disabled. Please use: cmake -DCMAKE_BUILD_TYPE=Debug -DZSWAG_ENABLE_COVERAGE=ON")
        set(ZSWAG_ENABLE_COVERAGE OFF CACHE BOOL "Coverage disabled due to build type" FORCE)
        return()
    endif()

    # Check for supported compiler
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Coverage is only supported with GCC or Clang. Current compiler: ${CMAKE_CXX_COMPILER_ID}")
        set(ZSWAG_ENABLE_COVERAGE OFF CACHE BOOL "Coverage disabled due to compiler" FORCE)
        return()
    endif()

    message(STATUS "Code coverage enabled for ${CMAKE_BUILD_TYPE} build with ${CMAKE_CXX_COMPILER_ID}")

    # Coverage compiler flags (as list for proper handling)
    set(COVERAGE_COMPILE_FLAGS -fprofile-arcs -ftest-coverage --coverage)
    set(COVERAGE_LINK_FLAGS --coverage)

    # For Clang, we may need to add -fprofile-instr-generate
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Standard coverage flags work for Clang too
        message(STATUS "Using Clang with gcov-compatible coverage")
    endif()

    # Find required tools
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)

    # Try to find gcov - first try the default, then versioned ones
    find_program(GCOV_PATH NAMES gcov gcov-13 gcov-12 gcov-11 gcov-10)

    # If still not found, try to match the GCC compiler version
    if(NOT GCOV_PATH AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Extract GCC version
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
            OUTPUT_VARIABLE GCC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        # Get major version
        string(REGEX MATCH "^[0-9]+" GCC_MAJOR_VERSION ${GCC_VERSION})
        # Try gcov with the matching version
        find_program(GCOV_PATH NAMES gcov-${GCC_MAJOR_VERSION})
        if(GCOV_PATH)
            message(STATUS "Found gcov matching GCC version: ${GCOV_PATH}")
        endif()
    endif()

    if(NOT LCOV_PATH)
        message(WARNING "lcov not found. Coverage report generation will not be available.")
        message(WARNING "Install with: sudo apt-get install lcov (Ubuntu/Debian) or brew install lcov (macOS)")
    endif()

    if(NOT GENHTML_PATH)
        message(WARNING "genhtml not found. HTML coverage report generation will not be available.")
    endif()

    if(NOT GCOV_PATH)
        message(WARNING "gcov not found. Coverage data collection will not be available.")
        message(WARNING "Install with: sudo apt-get install gcc (Ubuntu/Debian) or xcode-select --install (macOS)")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            # Extract GCC version for better error message
            execute_process(
                COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            string(REGEX MATCH "^[0-9]+" GCC_MAJOR_VERSION ${GCC_VERSION})
            message(WARNING "You are using GCC ${GCC_VERSION}. Try: sudo apt-get install gcc-${GCC_MAJOR_VERSION}")
            message(WARNING "Or create a symlink: sudo ln -s /usr/bin/gcov-${GCC_MAJOR_VERSION} /usr/bin/gcov")
        endif()
    endif()

    # Function to enable coverage for a target
    function(target_enable_coverage target_name)
        if(ZSWAG_ENABLE_COVERAGE)
            target_compile_options(${target_name} PRIVATE ${COVERAGE_COMPILE_FLAGS})
            target_link_libraries(${target_name} PRIVATE ${COVERAGE_LINK_FLAGS})

            message(STATUS "Coverage enabled for target: ${target_name}")
        endif()
    endfunction()

    # Coverage report directory
    set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage")
    set(COVERAGE_INFO_FILE "${COVERAGE_OUTPUT_DIR}/coverage.info")
    set(COVERAGE_CLEANED_FILE "${COVERAGE_OUTPUT_DIR}/coverage_cleaned.info")
    set(COVERAGE_HTML_DIR "${COVERAGE_OUTPUT_DIR}/html")

    # Patterns to include in coverage (library source files only)
    set(COVERAGE_INCLUDES
        '*/libs/httpcl/src/*'
        '*/libs/httpcl/include/*'
        '*/libs/zswagcl/src/*'
        '*/libs/zswagcl/include/*'
    )

    # Target: coverage-clean
    # Removes all coverage data files
    add_custom_target(coverage-clean
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${COVERAGE_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}
        COMMAND find ${CMAKE_BINARY_DIR} -type f -name '*.gcda' -delete
        COMMAND find ${CMAKE_BINARY_DIR} -type f -name '*.gcno' -delete
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Cleaning coverage data"
    )

    # Target: coverage-report
    # Generates coverage report from existing .gcda files
    if(LCOV_PATH AND GENHTML_PATH AND GCOV_PATH)
        add_custom_target(coverage-report
            # Create output directory
            COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}

            # Capture coverage data (with base directory for relative paths)
            COMMAND ${LCOV_PATH} --capture
                --directory ${CMAKE_BINARY_DIR}
                --output-file ${COVERAGE_INFO_FILE}
                --rc lcov_branch_coverage=1
                --gcov-tool ${GCOV_PATH}
                --base-directory ${CMAKE_SOURCE_DIR}

            # Extract only library source files (maintain base directory for relative paths)
            COMMAND ${LCOV_PATH} --extract ${COVERAGE_INFO_FILE}
                ${COVERAGE_INCLUDES}
                --output-file ${COVERAGE_CLEANED_FILE}
                --rc lcov_branch_coverage=1
                --base-directory ${CMAKE_SOURCE_DIR}

            # Generate HTML report
            COMMAND ${GENHTML_PATH} ${COVERAGE_CLEANED_FILE}
                --output-directory ${COVERAGE_HTML_DIR}
                --title "zswag Coverage Report"
                --legend
                --show-details
                --branch-coverage
                --rc genhtml_hi_limit=90
                --rc genhtml_med_limit=70

            # Print summary
            COMMAND ${LCOV_PATH} --list ${COVERAGE_CLEANED_FILE}

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report at ${COVERAGE_HTML_DIR}/index.html"
        )
    else()
        add_custom_target(coverage-report
            COMMAND ${CMAKE_COMMAND} -E echo "lcov and genhtml are required for coverage reports"
            COMMAND ${CMAKE_COMMAND} -E false
        )
    endif()

    # Target: coverage
    # Complete workflow: clean, run tests, generate report
    add_custom_target(coverage
        # Clean old coverage data
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target coverage-clean

        # Run tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure

        # Generate report
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target coverage-report

        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating coverage report"
    )

    # Make sure coverage depends on having tests built
    if(TARGET coverage)
        # These dependencies will be added when we update library CMakeLists
        # add_dependencies(coverage httpcl-test zswagcl-test)
    endif()

    # Print coverage information
    message(STATUS "Coverage configuration:")
    message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
    message(STATUS "  Output directory: ${COVERAGE_OUTPUT_DIR}")
    message(STATUS "  Targets available:")
    message(STATUS "    - coverage-clean: Remove coverage data")
    message(STATUS "    - coverage-report: Generate coverage report")
    message(STATUS "    - coverage: Run tests and generate report")

    if(LCOV_PATH AND GENHTML_PATH AND GCOV_PATH)
        message(STATUS "  Tools found:")
        message(STATUS "    - lcov: ${LCOV_PATH}")
        message(STATUS "    - genhtml: ${GENHTML_PATH}")
        message(STATUS "    - gcov: ${GCOV_PATH}")
    else()
        message(STATUS "  Missing tools - install lcov and gcc for full functionality")
    endif()
endif()
