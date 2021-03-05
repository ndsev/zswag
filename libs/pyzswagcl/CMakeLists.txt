project(pyzswagcl)

pybind11_add_module(pyzswagcl SHARED pyzswagcl.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(pyzswagcl PUBLIC /permissive-)
endif()

target_link_libraries(pyzswagcl
  PRIVATE
    zswagcl)

add_wheel(pyzswagcl
  VERSION ${ZSWAG_VERSION}
  DESCRIPTION "Python bindings for the zswag client library."
  TARGET_DEPENDENCIES
    zswagcl)
