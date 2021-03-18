project(pyzswagcl)

set(WHEEL_DEPLOY_DIRECTORY "${ZSWAG_DEPLOY_DIR}/wheel")

pybind11_add_module(pyzswagcl SHARED pyzswagcl.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(pyzswagcl PUBLIC /permissive-)
endif()

target_link_libraries(pyzswagcl
  PRIVATE
    zswagcl)

target_compile_features(pyzswagcl
  INTERFACE
    cxx_std_17)

add_wheel(pyzswagcl
  VERSION "${ZSWAG_VERSION}"
  DESCRIPTION "Python bindings for the zswag client library."
  TARGET_DEPENDENCIES
    zswagcl zsr)
