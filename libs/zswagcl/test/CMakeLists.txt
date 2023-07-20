project(zswagcl-test)

find_package(Catch2 CONFIG REQUIRED)

add_zserio_library(zswagcl-test-zs
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ENTRY "service_client_test.zs"
  WITH_REFLECTION)

add_executable(zswagcl-test
  src/main.cpp
  src/oaclient.cpp
  src/openapi-parameter-helper.cpp
  src/base64.cpp)

target_link_libraries(zswagcl-test
  PUBLIC
    zswagcl-test-zs
    zswagcl
    Catch2::Catch2WithMain)

target_compile_definitions(zswagcl-test
  PRIVATE
    -DTESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata/")

if (ZSWAG_ENABLE_TESTING)
  add_test(NAME zswagcl-test
    COMMAND "$<TARGET_FILE:zswagcl-test>")
endif()

if(APPLE)
  add_custom_command(TARGET zswagcl-test POST_BUILD
    COMMAND install_name_tool -add_rpath "${OPENSSL_INCLUDE_DIR}/../lib" "$<TARGET_FILE:zswagcl-test>")
endif()