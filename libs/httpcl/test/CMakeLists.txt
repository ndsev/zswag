project(httpcl-test)

add_executable(httpcl-test
  src/main.cpp
  src/uri.cpp
  src/http-client.cpp
  src/http-settings.cpp
  src/log.cpp
  src/http-settings-test.cpp
  src/oauth1-signature-test.cpp)

target_link_libraries(httpcl-test
  PUBLIC
    httpcl
    Catch2::Catch2WithMain)

add_executable(http2-multiplex-client
  src/http2-multiplex-client.cpp)

target_link_libraries(http2-multiplex-client
  PUBLIC
    httpcl)

# On Windows, ensure OpenSSL build completes before linking test executable
if(WIN32)
    add_dependencies(httpcl-test openssl_build)
    add_dependencies(http2-multiplex-client openssl_build)
endif()

if (ZSWAG_ENABLE_TESTING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME httpcl-test
    COMMAND "$<TARGET_FILE:httpcl-test>")
  add_test(NAME http2-multiplex-test
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/http2_multiplex_test.py" "$<TARGET_FILE:http2-multiplex-client>")
  set_tests_properties(http2-multiplex-test PROPERTIES SKIP_RETURN_CODE 77)
endif()
