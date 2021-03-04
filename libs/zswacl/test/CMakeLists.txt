project(zsr-service-client-test)

add_zserio_module(zs-service-client-test
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src"
  ENTRY "service_client_test.zs"
  TOP_LEVEL_PKG "service_client_test"
  SUBDIR_DEPTH 1)

add_executable(zsr-service-client-test
  src/main.cpp
  src/http-service.cpp
  src/base64.cpp)

target_link_libraries(zsr-service-client-test
  PUBLIC
    zsr-service-client
    zs-service-client-test
    zs-service-client-test-reflection
    zsr
    Catch2::Catch2)

target_compile_definitions(zsr-service-client-test
  PRIVATE
    -DTESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata/")

include(CTest)
add_test(NAME zsr-service-client-test
  COMMAND "$<TARGET_FILE:zsr-service-client-test>")
