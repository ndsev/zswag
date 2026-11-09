#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <fstream>

#include "zswagcl/private/openapi-parser.hpp"
#include "httpcl/http-settings.hpp"
#include "py-openapi-client.h"
#include "stx/format.h"

namespace py = pybind11;
using namespace py::literals;
using namespace std::string_literals;

PYBIND11_MODULE(pyzswagcl, m)
{
    using namespace zswagcl;
    using namespace httpcl;

    m.doc() = R"doc(
Native zswag OpenAPI client bindings.

The module exposes the C++ HTTP/OpenAPI client to Python. It is normally
imported through :mod:`zswag`, which combines these native client bindings with
the pure-Python server-side helpers.
)doc";

    auto parameterMap = py::bind_map<std::map<std::string, OpenAPIConfig::Parameter>>(m, "ParameterMap");
    parameterMap.attr("__doc__") = R"doc(
Mutable mapping from OpenAPI parameter names to :class:`OAParam` descriptors.

Instances are returned by :attr:`OAMethod.parameters` and describe how a zserio
request field is placed into the HTTP request for one service method.
)doc";
    py::implicitly_convertible<py::dict, std::map<std::string, OpenAPIConfig::Parameter>>();

    auto headerMap = py::bind_map<PyOpenApiClient::Headers>(m, "HeaderMap");
    headerMap.attr("__doc__") = R"doc(
Mutable mapping from HTTP header names to header values.

The map type is accepted wherever the native client expects explicit request or
configuration headers.
)doc";
    py::implicitly_convertible<py::dict, PyOpenApiClient::Headers>();

    auto httpError = py::register_exception<httpcl::IHttpClient::Error>(m, "HTTPError");
    httpError.attr("__doc__") = R"doc(
Raised when OpenAPI discovery or a service call rejects the HTTP result.

Service calls require status 200; discovery accepts any 2xx. Curl transport
failures appear as status zero and become this exception at the OpenAPI layer.
Configuration and unsatisfied security requirements may raise RuntimeError
instead. This binding exposes the exception message, not the native result
member containing status and response content.
)doc";

    ///////////////////////////////////////////////////////////////////////////
    // ParameterLocation

    py::enum_<OpenAPIConfig::ParameterLocation>(m, "OAParamLocation", py::arithmetic(), R"doc(
Location of an OpenAPI parameter in an outgoing HTTP request.
)doc")
            .value("PATH", OpenAPIConfig::ParameterLocation::Path, "Parameter value is substituted into the URL path.")
            .value("QUERY", OpenAPIConfig::ParameterLocation::Query,
                "Parameter value is appended to the URL query string.")
            .value("HEADER", OpenAPIConfig::ParameterLocation::Header, "Parameter value is sent as an HTTP header.")
            ;

    ///////////////////////////////////////////////////////////////////////////
    // ParameterFormat

    py::enum_<OpenAPIConfig::Parameter::Format>(m, "OAParamFormat", py::arithmetic(), R"doc(
Wire encoding used for a zserio request value exposed as an HTTP parameter.
)doc")
            .value("STRING", OpenAPIConfig::Parameter::Format::String, "Use the scalar value's textual representation.")
            .value("HEX", OpenAPIConfig::Parameter::Format::Hex, "Encode binary parameter data as hexadecimal text.")
            .value("BASE64", OpenAPIConfig::Parameter::Format::Base64,
                "Encode binary parameter data as standard Base64.")
            .value("BASE64URL", OpenAPIConfig::Parameter::Format::Base64url,
                "Encode binary parameter data as URL-safe Base64.")
            .value("BINARY", OpenAPIConfig::Parameter::Format::Binary,
                "Send binary data without an additional text encoding.")
            ;

    ///////////////////////////////////////////////////////////////////////////
    // Parameter

    py::class_<OpenAPIConfig::Parameter>(m, "OAParam", R"doc(
Description of one OpenAPI parameter for a zswag service method.

The descriptor maps an OpenAPI parameter name to a zserio request field, the
parameter's HTTP location, the optional default value, and the value encoding.
)doc")
        .def_readonly(
            "location",
            &OpenAPIConfig::Parameter::location,
            "Where the parameter is placed in the HTTP request.")
        .def_readonly(
            "field",
            &OpenAPIConfig::Parameter::field,
            "Dotted zserio request-field path read to populate this parameter.")
        .def_readonly(
            "default_value",
            &OpenAPIConfig::Parameter::defaultValue,
            "Fallback value; the current parser leaves this empty instead of reading schema.default.")
        .def_readonly(
            "format",
            &OpenAPIConfig::Parameter::format,
            "Wire encoding applied to the extracted request-field value.");

    ///////////////////////////////////////////////////////////////////////////
    // Path

    py::class_<OpenAPIConfig::Path>(m, "OAMethod", R"doc(
Resolved OpenAPI transport configuration for one zserio service method.

Objects are returned from :class:`OAConfig` by method name and are used by
:class:`OAClient` to construct the concrete HTTP request for a zserio call.
)doc")
        .def_readonly(
            "path",
            &OpenAPIConfig::Path::path,
            "URL path template for the service method.")
        .def_readonly(
            "http_method",
            &OpenAPIConfig::Path::httpMethod,
            "Uppercase HTTP method such as ``GET`` or ``POST``.")
        .def_readonly(
            "parameters",
            &OpenAPIConfig::Path::parameters,
            "Mapping from OpenAPI parameter names to :class:`OAParam` descriptors.")
        .def_readonly(
            "body_request_object",
            &OpenAPIConfig::Path::bodyRequestObject,
            "Whether the operation advertises a whole zserio request object as its body.");

    ///////////////////////////////////////////////////////////////////////////
    // httpcl::Config

    py::class_<httpcl::Config>(m, "HTTPConfig", R"doc(
Mutable HTTP transport configuration used by :class:`OAClient`.

Mutators return ``self`` for chaining, e.g.
``HTTPConfig().header("X-Trace", "1").bearer(token)``. Header/query calls append values,
including duplicate names; cookie calls preserve an existing name's value.
Pickling uses Config's YAML representation, which includes cleartext secrets
and collapses duplicate header/query names, so it is not a lossless snapshot.
)doc")
        .def(py::init<>(), "Create an empty HTTP transport configuration.")
        .def(
            "header",
            [](httpcl::Config& self, std::string const& key, std::string const& value)
            {
                self.headers.insert({key, value});
                return &self;
            },
            "Append an HTTP header, retaining duplicate names.",
            "key"_a,
            "val"_a)
        .def(
            "query",
            [](httpcl::Config& self, std::string const& key, std::string const& value)
            {
                self.query.insert({key, value});
                return &self;
            },
            "Append a query parameter, retaining duplicate names.",
            "key"_a,
            "val"_a)
        .def(
            "cookie",
            [](httpcl::Config& self, std::string const& key, std::string const& value)
            {
                self.cookies.insert({key, value});
                return &self;
            },
            "Add or keep a cookie sent with every request using this config.",
            "key"_a,
            "val"_a)
        .def(
            "bearer",
            [](httpcl::Config& self, std::string const& key)
            {
                self.headers.insert({"Authorization", stx::format("Bearer {}", key)});
                return &self;
            },
            "Append an ``Authorization: Bearer ...`` header without replacing existing values.",
            "token"_a)
        .def(
            "api_key",
            [](httpcl::Config& self, std::string const& key)
            {
                self.apiKey = key;
                return &self;
            },
            "Set the OpenAPI API-key value used by generated API-key security schemes.",
            "token"_a)
        .def(
            "basic_auth",
            [](httpcl::Config& self, std::string const& user, std::string const& pw)
            {
                self.auth = httpcl::Config::BasicAuthentication{user, pw, ""};
                return &self;
            },
            "Configure HTTP Basic authentication credentials.",
            "user"_a,
            "pw"_a)
        .def(
            "proxy",
            [](httpcl::Config& self,
               std::string const& host,
               int port,
               std::string const& user = {},
               std::string const& pw = {})
            {
                self.proxy = httpcl::Config::Proxy{host, port, user, pw, ""};
                return &self;
            },
            R"doc(
Configure an HTTP proxy for requests using this config.

``user`` and ``pw`` are optional proxy credentials. Leave them empty for an
unauthenticated proxy.
)doc",
            "host"_a,
            "port"_a,
            "user"_a = "",
            "pw"_a = "")
        .def(py::pickle(
            [](httpcl::Config const& self)
            {
                return py::make_tuple(self.toYaml());
            },
            [](py::tuple const& t)
            {
                return Config(t[0].cast<std::string>());
            }));

    ///////////////////////////////////////////////////////////////////////////
    // OpenAPIConfig
    py::class_<OpenAPIConfig>(m, "OAConfig", R"doc(
Parsed OpenAPI configuration used by the native zswag client.

The object behaves like a read-only mapping from zserio service method name to
:class:`OAMethod`. It also exposes the original YAML content and the resolved
server URLs from the OpenAPI document.
)doc")
        .def(
            "__contains__",
            [](const OpenAPIConfig& self, std::string const& methodName)
            {
                return self.methodPath.find(methodName) != self.methodPath.end();
            },
            "Return ``True`` if the OpenAPI document contains the service method.",
            py::is_operator(),
            "method_name"_a)
        .def(
            "__getitem__",
            [](const OpenAPIConfig& self, std::string const& methodName)
            {
                auto it = self.methodPath.find(methodName);
                if (it != self.methodPath.end())
                    return it->second;
                throw std::runtime_error(
                    "Could not find OpenAPI config for method name "s + methodName);
            },
            "Return a copy of the method descriptor; missing names raise RuntimeError.",
            py::is_operator(),
            py::return_value_policy::reference_internal,
            "method_name"_a)
        .def_readonly(
            "content",
            &OpenAPIConfig::content,
            "Raw OpenAPI YAML/JSON document content that was parsed.")
        .def_property_readonly(
            "servers",
            [](const OpenAPIConfig& self) -> std::vector<std::string>
            {
                std::vector<std::string> result;
                for (auto const& uri : self.servers)
                    result.emplace_back(uri.build());
                return result;
            },
            py::return_value_policy::automatic,
            "Build advertised server URLs; unresolved relative entries from local parsing raise RuntimeError.");

    m.def(
        "parse_openapi_config",
        [](std::string const& path)
        {
            std::ifstream ifs;
            ifs.open(path);
            return parseOpenAPIConfig(ifs);
        },
        R"doc(
Parse the supported zswag OpenAPI subset from a local YAML/JSON file.

Returns an :class:`OAConfig`, the configuration type used by :class:`OAClient`.

Relative server entries stay unresolved and an absent server list stays empty.
File/YAML/configuration failures raise; this does not perform general OpenAPI
validation or resolve references from other documents.
)doc",
        py::return_value_policy::move,
        "path"_a);

    m.def(
        "fetch_openapi_config",
        [](std::string const& url)
        {
            CurlHttpClient httpClient;
            return fetchOpenAPIConfig(url, httpClient);
        },
        R"doc(
Fetch and parse an OpenAPI YAML/JSON document synchronously from *url*.

The temporary native transport applies HTTP_SETTINGS_FILE entries, including
configured proxy/authentication, and resolves relative server URLs against the
document URL. This binding keeps the GIL. Use OAClient with HTTPConfig for
additional per-client settings and a transport reused across service calls.
)doc",
        py::return_value_policy::move,
        "url"_a);

    ///////////////////////////////////////////////////////////////////////////
    // Global Constants
    m.attr("ZSERIO_OBJECT_CONTENT_TYPE") = py::str(ZSERIO_OBJECT_CONTENT_TYPE);
    m.attr("ZSERIO_REQUEST_PART") = py::str(ZSERIO_REQUEST_PART);
    m.attr("ZSERIO_REQUEST_PART_WHOLE") = py::str(ZSERIO_REQUEST_PART_WHOLE);

    ///////////////////////////////////////////////////////////////////////////
    // PyOpenApiClient
    PyOpenApiClient::bind(m);
}
