project(httpcl-test)

add_executable(httpcl-test
  src/main.cpp
  src/uri.cpp)

target_link_libraries(httpcl-test
  PUBLIC
    httpcl
    Catch2::Catch2WithMain)

if (ZSWAG_ENABLE_TESTING)
  add_test(NAME httpcl-test
    COMMAND "$<TARGET_FILE:httpcl-test>")
endif()

if(APPLE)
  add_custom_command(TARGET httpcl-test POST_BUILD
    COMMAND install_name_tool -add_rpath "${OPENSSL_INCLUDE_DIR}/../lib" "$<TARGET_FILE:httpcl-test>")
endif()