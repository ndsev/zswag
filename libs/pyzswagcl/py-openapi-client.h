#include "zswagcl/openapi-client.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

class PyOpenApiClient
{
public:
    static void bind(py::module_& m);

    PyOpenApiClient(std::string const& openApiUrl, bool isLocalFile);

    std::vector<uint8_t> callMethod(
        const std::string& methodName,
        py::bytearray& requestData,
        py::handle context);

private:
    std::unique_ptr<zswagcl::OpenAPIClient> client_;
};
