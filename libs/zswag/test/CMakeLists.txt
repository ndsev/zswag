project(zswag-cpp-calc-test)

add_zserio_module(${PROJECT_NAME}-zsr
  ROOT "${CMAKE_CURRENT_SOURCE_DIR}/calc"
  ENTRY calculator.zs
  TOP_LEVEL_PKG calculator
  SUBDIR_DEPTH 0)

add_executable(${PROJECT_NAME} client.cpp)

target_link_libraries(${PROJECT_NAME}
  ${PROJECT_NAME}-zsr-reflection zswagcl stx zsr)
