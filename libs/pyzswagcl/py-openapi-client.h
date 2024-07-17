#include "zswagcl/private/openapi-client.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <map>

namespace py = pybind11;

class PyOpenApiClient
{
public:
    static void bind(py::module_& m);

    using Headers = std::map<std::string, std::string>;

    PyOpenApiClient(std::string const& openApiUrl,
                    bool isLocalFile,
                    httpcl::Config const& config,
                    std::optional<std::string> apiKey,
                    std::optional<std::string> bearer,
                    std::optional<uint32_t> serverIndex);

    std::vector<uint8_t> callMethod(
        const std::string& methodName,
        py::object request,
        py::object unused);

private:
    std::unique_ptr<zswagcl::OpenAPIClient> client_;
};
