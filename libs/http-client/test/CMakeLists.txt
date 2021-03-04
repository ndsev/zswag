project(http-client-test)

add_executable(http-client-test
  src/main.cpp
  src/uri.cpp)

target_link_libraries(http-client-test
  PUBLIC
  http-client
  zsr
  Catch2::Catch2)

include(CTest)
add_test(NAME http-client-test
  COMMAND "$<TARGET_FILE:http-client-test>")
