#include "zswagcl/private/openapi-client.hpp"
#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <map>

namespace py = nanobind;

/** Python zserio adapter owning one native client; bound calls retain the GIL. */
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

    /** Borrow the generated request wrapper for this blocking call and return owned bytes. */
    std::vector<uint8_t>
    callMethod(const std::string& methodName, py::object request, py::object unused);

private:
    std::unique_ptr<zswagcl::OpenAPIClient> client_;
};
