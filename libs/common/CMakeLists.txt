add_library(zswag-common INTERFACE)

target_include_directories(zswag-common
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

if(ZSWAG_USE_CPP14)
    target_compile_features(zswag-common INTERFACE cxx_std_14)
else()
    target_compile_features(zswag-common INTERFACE cxx_std_17)
endif()

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp")

if(ZSWAG_ENABLE_TESTING)
    add_executable(compat_test test/compat_test.cpp)
    target_link_libraries(compat_test
        PRIVATE
            zswag-common
            Catch2::Catch2WithMain)
    
    add_test(NAME compat_test
        COMMAND "$<TARGET_FILE:compat_test>")
endif() 