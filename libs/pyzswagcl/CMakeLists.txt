project(pyzswagcl)

pybind11_add_module(pyzswagcl
  MODULE
    py-zswagcl.cpp
    py-openapi-client.cpp
    py-openapi-client.h)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(pyzswagcl PUBLIC /permissive-)
endif()

target_link_libraries(pyzswagcl
  PRIVATE
    zswagcl
    httpcl)

target_compile_features(pyzswagcl
  INTERFACE
    cxx_std_17)

add_wheel(pyzswagcl
  AUTHOR "Navigation Data Standard e.V."
  URL "https://github.com/ndsev/zswag"
  VERSION "${ZSWAG_VERSION}"
  DESCRIPTION "Python bindings for the zswag client library."
  TARGET_DEPENDENCIES
    zswagcl speedyj httpcl)
