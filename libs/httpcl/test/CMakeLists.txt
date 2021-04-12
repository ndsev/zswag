project(httpcl-test)

add_executable(httpcl-test
  src/main.cpp
  src/uri.cpp)

target_link_libraries(httpcl-test
  PUBLIC
    httpcl
    zsr
    Catch2::Catch2)

if (ZSWAG_ENABLE_TESTING)
  add_test(NAME httpcl-test
    COMMAND "$<TARGET_FILE:httpcl-test>")
endif()
