project(httpcl-test)

add_executable(httpcl-test
  src/main.cpp
  src/uri.cpp)

target_link_libraries(httpcl-test
  PUBLIC
    httpcl
    Catch2::Catch2WithMain)

# On Windows, ensure OpenSSL build completes before linking test executable
if(WIN32)
    add_dependencies(httpcl-test openssl_build)
endif()

if (ZSWAG_ENABLE_TESTING)
  add_test(NAME httpcl-test
    COMMAND "$<TARGET_FILE:httpcl-test>")
endif()
