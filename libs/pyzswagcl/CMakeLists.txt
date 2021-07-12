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

if (MSVC)
  # Required because cpp-httplib speaks https via OpenSSL
  set (DEPLOY_FILES
    "${OPENSSL_INCLUDE_DIR}/../libcrypto-1_1-x64.dll"
    "${OPENSSL_INCLUDE_DIR}/../libssl-1_1-x64.dll")
endif()

add_wheel(pyzswagcl
  AUTHOR "Klebert Engineering"
  URL "https://github.com/klebert-engineering/zswag"
  VERSION "${ZSWAG_VERSION}"
  DESCRIPTION "Python bindings for the zswag client library."
  TARGET_DEPENDENCIES
    zswagcl zsr speedyj httpcl
  DEPLOY_FILES
    ${DEPLOY_FILES})
