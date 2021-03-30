project(zswag-cpp-calc-test)

add_zserio_module(${PROJECT_NAME}-zsr
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}/calc"
  ENTRY calculator.zs
  TOP_LEVEL_PKG calculator
  SUBDIR_DEPTH 0)

add_executable(${PROJECT_NAME} client.cpp)

target_link_libraries(${PROJECT_NAME}
  ${PROJECT_NAME}-zsr-reflection zswagcl stx zsr)

add_test(
    NAME
      zswag-server-integration
    WORKING_DIRECTORY
      "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND
      bash "test_integration.bash"
        -w "${WHEEL_DEPLOY_DIRECTORY}"
        -b "python -m zswag.test.calc server localhost:16161"
        -f "$<TARGET_FILE:${PROJECT_NAME}> http://localhost:16161/openapi.json"
        -f "python -m zswag.test.calc client localhost:16161")
