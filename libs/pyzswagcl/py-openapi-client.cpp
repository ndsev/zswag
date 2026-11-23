#include "py-openapi-client.h"
#include "httpcl/http-client.hpp"
#include "stx/format.h"
#include "stx/string.h"
#include <fstream>
#include <iostream>

using namespace py::literals;
using namespace std::string_literals;
using namespace zswagcl;
using namespace httpcl;

namespace
{
    Any valueFromPyObject(py::handle value) {
        if (py::isinstance<py::int_>(value)) {
            int64_t signedValue;
            if (py::try_cast(value, signedValue))
                return signedValue;
            return py::cast<uint64_t>(value);
        }

        if (py::hasattr(value, "value")) // enum item
            return valueFromPyObject(value.attr("value"));
        if (py::isinstance<py::float_>(value))
            return py::cast<double>(value);
        if (py::isinstance<py::str>(value))
            return py::cast<std::string>(value);

        throw py::type_error(stx::format("Unsupported request value: {}",
                                        py::cast<std::string>(py::repr(value))).c_str());
    }

    std::vector<Any> valuesFromPyArray(py::handle value) {
        std::vector<Any> result;
        result.reserve(py::len(value));
        for (py::handle item : value)
            result.push_back(valueFromPyObject(item));
        return result;
    }
}

void PyOpenApiClient::bind(py::module_& m) {
    py::class_<PyOpenApiClient>(m, "OAClient", R"doc(
Native OpenAPI client for zserio services.

``OAClient`` loads an OpenAPI document, selects one configured server URL, and
uses the generated zserio Python request wrappers to call service methods over
HTTP. The public Python class adds the zserio ServiceInterface base.
)doc")
            .def(
                py::init<
                    std::string,
                    bool,
                    httpcl::Config,
                    std::optional<std::string>,
                    std::optional<std::string>,
                    std::optional<uint32_t>>(),
                R"doc(
Create a blocking OpenAPI client from a URL or local YAML/JSON document.

``url`` is the OpenAPI document URL, or a local file path when ``is_local_file``
is true. ``config`` supplies reusable HTTP settings such as headers, query
parameters, proxy, and authentication.

``config`` is copied. ``api_key`` replaces its API-key shortcut; ``bearer``
appends an Authorization header, including when one is already configured.
``server_index`` selects an advertised server (default zero); invalid indices
raise RuntimeError. Local documents need an absolute selected server URL.
Remote loading resolves relative servers and supplies the origin root when
none are declared. Construction retains the GIL while fetching/parsing and
owns one reusable native HTTP transport for subsequent calls.
)doc",
                "url"_a,
                "is_local_file"_a = false,
                "config"_a = httpcl::Config(),
                "api_key"_a = std::optional<std::string>(),
                "bearer"_a = std::optional<std::string>(),
                "server_index"_a = std::optional<uint32_t>())
            // zserio >= 2.3.0
            .def(
                "call_method",
                &PyOpenApiClient::callMethod,
                R"doc(
Call a service synchronously and return the serialized response as byte values.

``method_name`` must match an OpenAPI operation. The generated request wrapper
supplies ``byte_array`` for whole-request mappings, or ``zserio_object`` for
field mappings. ``unused`` preserves the zserio ServiceInterface signature.
The binding keeps the GIL while waiting and returns a list of integers from
the native byte vector. Unknown methods or field/conversion failures raise;
only HTTP 200 succeeds, with other statuses reported as HTTPError.
)doc",
                "method_name"_a,
                "request"_a,
                "unused"_a.none())
            .def(
                "config",
                [](PyOpenApiClient const& self) -> OpenAPIConfig const&
                {
                    return self.client_->config_;
                },
                R"doc(
Return the parsed OpenAPI configuration owned by this client.

The returned :class:`OAConfig` stays valid as long as the client object exists.
)doc",
                py::rv_policy::reference_internal);

}

PyOpenApiClient::PyOpenApiClient(std::string const& openApiUrl,
                                 bool isLocalFile,
                                 httpcl::Config const& config,
                                 std::optional<std::string> apiKey,
                                 std::optional<std::string> bearer,
                                 std::optional<uint32_t> serverIndex)
{
    auto httpConfig = config; // writable copy
    if (apiKey)
        httpConfig.apiKey = std::move(apiKey);
    if (bearer)
        httpConfig.headers.insert({"Authorization", stx::format("Bearer {}", *bearer)});
    auto httpClient = std::make_unique<CurlHttpClient>();
    OpenAPIConfig openApiConfig = [&](){
        if (isLocalFile) {
            std::ifstream fs(openApiUrl);
            return parseOpenAPIConfig(fs);
        }
        return fetchOpenAPIConfig(openApiUrl, *httpClient, httpConfig);
    }();

    client_ = std::make_unique<OpenAPIClient>(
        openApiConfig,
        httpConfig,
        std::move(httpClient),
        serverIndex ? *serverIndex : 0);
}

std::vector<uint8_t> PyOpenApiClient::callMethod(
        const std::string& methodName,
        py::object request,
        py::object unused)
{
    if (request.is_none()) {
        throw std::runtime_error("The request argument is None!");
    }

    auto response = client_->call(methodName, [&](const std::string& parameter, const std::string& field, ParameterValueHelper& helper)
    {
        if (field == ZSERIO_REQUEST_PART_WHOLE) {
            // Python's buffer protocol is not part of the 3.10 limited API.
            // Convert bytes-like request wrappers without losing embedded NULs.
            py::object buffer = request.attr("byte_array");
            py::bytes requestData(buffer);
            auto data = static_cast<const uint8_t*>(requestData.data());
            return helper.binary(std::vector<uint8_t>(data, data + requestData.size()));
        }

        auto parts = stx::split<std::vector<std::string>>(field, ".");
        auto currentField = parts.begin();
        py::object value = request.attr("zserio_object");

        while (currentField != parts.end()) {
            auto internalFieldName = stx::format("_{}_", *currentField);
            if (!py::hasattr(value, internalFieldName.c_str())) {
                throw std::runtime_error(stx::format("Could not find request field {} in method {}.",
                    stx::join(parts.begin(), currentField + 1, "."),
                    methodName));
            }
            value = value.attr(internalFieldName.c_str());
            ++currentField;
        }

        if (PySequence_Check(value.ptr()) && !py::isinstance<py::str>(value)) {
            return helper.array(valuesFromPyArray(value));
        }

        return helper.value(valueFromPyObject(value));
    });

    std::vector<uint8_t> responseData;
    responseData.assign(response.begin(), response.end());
    return responseData;
}
