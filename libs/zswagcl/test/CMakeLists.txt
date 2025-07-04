project(zswagcl-test)

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

# On Windows, ensure OpenSSL build completes before linking test executable
if(WIN32)
    add_dependencies(zswagcl-test openssl_build)
endif()

if (ZSWAG_ENABLE_TESTING)
  add_test(NAME zswagcl-test
    COMMAND "$<TARGET_FILE:zswagcl-test>")
endif()
