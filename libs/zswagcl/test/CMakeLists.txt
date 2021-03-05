project(zswagcl-test)

add_zserio_module(zscl-test
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ENTRY "service_client_test.zs"
  TOP_LEVEL_PKG "service_client_test"
  SUBDIR_DEPTH 1)

add_executable(zswagcl-test
  src/main.cpp
  src/http-service.cpp
  src/base64.cpp)

target_link_libraries(zswagcl-test
  PUBLIC
    zscl-test
    zscl-test-reflection
    zswagcl
    zsr
    Catch2::Catch2)

target_compile_definitions(zswagcl-test
  PRIVATE
    -DTESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata/")

include(CTest)
add_test(NAME zswagcl-test
  COMMAND "$<TARGET_FILE:zswagcl-test>")
