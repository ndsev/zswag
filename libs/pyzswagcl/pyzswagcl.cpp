#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <fstream>

#include "zswagcl/openapi-parser.hpp"
#include "httpcl/http-settings.hpp"

namespace py = pybind11;
using namespace py::literals;
using namespace std::string_literals;

PYBIND11_MODULE(pyzswagcl, m)
{
    using namespace zswagcl;
    using namespace httpcl;

    py::bind_map<std::map<std::string, OpenAPIConfig::Parameter>>(m, "ParameterMap");
    py::implicitly_convertible<py::dict, std::map<std::string, OpenAPIConfig::Parameter>>();

    ///////////////////////////////////////////////////////////////////////////
    // ParameterLocation

    py::enum_<OpenAPIConfig::Parameter::Location>(m, "OpenApiConfigParamLocation", py::arithmetic())
            .value("PATH", OpenAPIConfig::Parameter::Location::Path)
            .value("QUERY", OpenAPIConfig::Parameter::Location::Query)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // ParameterFormat

    py::enum_<OpenAPIConfig::Parameter::Format>(m, "OpenApiConfigParamFormat", py::arithmetic())
            .value("STRING", OpenAPIConfig::Parameter::Format::String)
            .value("HEX", OpenAPIConfig::Parameter::Format::Hex)
            .value("BASE64", OpenAPIConfig::Parameter::Format::Base64)
            .value("BASE64URL", OpenAPIConfig::Parameter::Format::Base64url)
            .value("BINARY", OpenAPIConfig::Parameter::Format::Binary)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // Parameter

    py::class_<OpenAPIConfig::Parameter>(m, "OpenApiConfigParam")
            .def_readonly("location", &OpenAPIConfig::Parameter::location)
            .def_readonly("field", &OpenAPIConfig::Parameter::field)
            .def_readonly("default_value", &OpenAPIConfig::Parameter::defaultValue)
            .def_readonly("format", &OpenAPIConfig::Parameter::format)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // Path

    py::class_<OpenAPIConfig::Path>(m, "OpenApiConfigMethod")
            .def_readonly("path", &OpenAPIConfig::Path::path)
            .def_readonly("http_method", &OpenAPIConfig::Path::httpMethod)
            .def_readonly("parameters", &OpenAPIConfig::Path::parameters)
            .def_readonly("body_request_object", &OpenAPIConfig::Path::bodyRequestObject)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // HTTPService::Config
    py::class_<OpenAPIConfig>(m, "OpenAPIConfig")
            .def("__contains__", [](const OpenAPIConfig& self, std::string const& methodName) {
                return self.methodPath.find(methodName) != self.methodPath.end();
            }, py::is_operator(), "method_name"_a)

            .def("__getitem__", [](const OpenAPIConfig& self, std::string const& methodName) {
                auto it = self.methodPath.find(methodName);
                if (it != self.methodPath.end())
                    return it->second;
                else
                    throw std::runtime_error(
                        "Could not find OpenAPI config for method name "s+methodName);
            }, py::is_operator(), py::return_value_policy::reference_internal, "method_name"_a)
            ;

    m.def("parse_openapi_config", [](std::string const& path){
        std::ifstream ifs;
        ifs.open(path);
        return parseOpenAPIConfig(ifs);
    }, py::return_value_policy::move, "path"_a);

    m.def("fetch_openapi_config", [](std::string const& url){
        HttpLibHttpClient httpClient;
        return fetchOpenAPIConfig(url, httpClient);
    }, py::return_value_policy::move, "url"_a);

    m.attr("ZSERIO_OBJECT_CONTENT_TYPE") = py::str(ZSERIO_OBJECT_CONTENT_TYPE);
    m.attr("ZSERIO_REQUEST_PART") = py::str(ZSERIO_REQUEST_PART);
    m.attr("ZSERIO_REQUEST_PART_WHOLE") = py::str(ZSERIO_REQUEST_PART_WHOLE);
}
