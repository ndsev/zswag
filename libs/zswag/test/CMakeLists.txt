project(zswag-cpp-calc-test)

add_zserio_module(${PROJECT_NAME}-zsr
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}/calc"
  ENTRY calculator.zs
  TOP_LEVEL_PKG calculator
  SUBDIR_DEPTH 0)

add_executable(${PROJECT_NAME} client.cpp)

target_link_libraries(${PROJECT_NAME}
  ${PROJECT_NAME}-zsr-reflection zswagcl stx zsr)

add_test(zswag-server-integration
    bash "${CMAKE_CURRENT_SOURCE_DIR}/test_integration.bash"
      -w "${WHEEL_DEPLOY_DIRECTORY}"
      -b "python -m zswag.test.calc server localhost:8085"
      -f "${ZSWAG_DEPLOY_DIR}/${PROJECT_NAME} http://localhost:8085/openapi.json")
