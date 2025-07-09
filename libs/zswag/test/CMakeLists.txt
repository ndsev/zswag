project(zswag-cpp-calc-test)

add_zserio_library(${PROJECT_NAME}-zserio
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}/calc"
  ENTRY calculator.zs
  WITH_REFLECTION)

add_executable(${PROJECT_NAME} client.cpp)

target_link_libraries(${PROJECT_NAME}
  ${PROJECT_NAME}-zserio zswagcl stx)

# On Windows, ensure OpenSSL build completes before linking test executable
if(WIN32)
    add_dependencies(${PROJECT_NAME} openssl_build)
endif()

if (ZSWAG_ENABLE_TESTING)
  add_wheel_test(zswag-server-integration
    WORKING_DIRECTORY
      "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMANDS
      -f "pip install ${WHEEL_DEPLOY_DIRECTORY}/zswag-*.whl"
      -b "python -m zswag.test.calc server 127.0.0.1:16161"
      -f "$<TARGET_FILE:${PROJECT_NAME}> http://127.0.0.1:16161/openapi.json"
      -f "python -m zswag.test.calc client 127.0.0.1:16161")

  add_test(
    NAME
      zswag-openapi-generator
    WORKING_DIRECTORY
      "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND
      bash "test_openapi_generator.bash"
      -w "${WHEEL_DEPLOY_DIRECTORY}")
endif()
