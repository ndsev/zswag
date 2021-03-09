#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <fstream>

#include "zswagcl/openapi-parser.hpp"
#include "httpcl/http-settings.hpp"

namespace py = pybind11;
using namespace py::literals;

PYBIND11_MODULE(pyzswagcl, m)
{
    using namespace zswagcl;
    using namespace httpcl;
    using OpenApiConfig = HTTPService::Config;

    py::bind_map<std::map<std::string, OpenApiConfig::Parameter>>(m, "ParameterMap");
    py::implicitly_convertible<py::dict, std::map<std::string, OpenApiConfig::Parameter>>();

    ///////////////////////////////////////////////////////////////////////////
    // ParameterLocation

    py::enum_<OpenApiConfig::Parameter::Location>(m, "OpenApiConfigParamLocation", py::arithmetic())
            .value("PATH", OpenApiConfig::Parameter::Location::Path)
            .value("QUERY", OpenApiConfig::Parameter::Location::Query)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // ParameterFormat

    py::enum_<OpenApiConfig::Parameter::Format>(m, "OpenApiConfigParamFormat", py::arithmetic())
            .value("STRING", OpenApiConfig::Parameter::Format::String)
            .value("HEX", OpenApiConfig::Parameter::Format::Hex)
            .value("BASE64", OpenApiConfig::Parameter::Format::Base64)
            .value("BASE64URL", OpenApiConfig::Parameter::Format::Base64url)
            .value("BINARY", OpenApiConfig::Parameter::Format::Binary)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // Parameter

    py::class_<OpenApiConfig::Parameter>(m, "OpenApiConfigParam")
            .def_readonly("location", &OpenApiConfig::Parameter::location)
            .def_readonly("field", &OpenApiConfig::Parameter::field)
            .def_readonly("default_value", &OpenApiConfig::Parameter::defaultValue)
            .def_readonly("format", &OpenApiConfig::Parameter::format)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // Path

    py::class_<OpenApiConfig::Path>(m, "OpenApiConfigMethod")
            .def_readonly("path", &OpenApiConfig::Path::path)
            .def_readonly("http_method", &OpenApiConfig::Path::httpMethod)
            .def_readonly("parameters", &OpenApiConfig::Path::parameters)
            .def_readonly("body_request_object", &OpenApiConfig::Path::bodyRequestObject)
            ;

    ///////////////////////////////////////////////////////////////////////////
    // HTTPService::Config
    py::class_<OpenApiConfig>(m, "OpenApiConfig")
            .def("__contains__", [](const OpenApiConfig& self, std::string const& methodName) {
                return self.methodPath.find(methodName) != self.methodPath.end();
            }, py::is_operator(), "method_name"_a)

            .def("__getitem__", [](const OpenApiConfig& self, std::string const& methodName) {
                auto it = self.methodPath.find(methodName);
                if (it != self.methodPath.end())
                    return it->second;
                else
                    throw std::runtime_error(
                        std::string("Could not find OpenAPi config for method name ")+methodName);
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
