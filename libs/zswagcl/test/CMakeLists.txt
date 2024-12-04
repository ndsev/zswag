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
  PRIVATE
    zswagcl
    zswag-common
    zswagcl-test-zs
    Catch2::Catch2WithMain)

target_include_directories(zswagcl-test
  PRIVATE
    src)

target_compile_definitions(zswagcl-test
  PRIVATE
    -DTESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata/")

if(ZSWAG_USE_CPP14)
  target_compile_features(zswagcl-test PUBLIC cxx_std_14)
else()
  target_compile_features(zswagcl-test PUBLIC cxx_std_17)
endif()

if(ZSWAG_ENABLE_TESTING)
  add_test(NAME zswagcl-test
    COMMAND "$<TARGET_FILE:zswagcl-test>")
endif()
