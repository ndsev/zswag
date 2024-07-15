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
    Any valueFromPyObject(PyObject* value) {
        if (PyLong_Check(value)) {
            auto i64 = static_cast<int64_t>(PyLong_AsLongLong(value));
            if (i64 == -1 && PyErr_Occurred()) {
                return static_cast<uint64_t>(PyLong_AsUnsignedLongLong(value));
            }
            return i64;
        }

        if (PyObject_HasAttrString(value, "value")) { // enum item
            return valueFromPyObject(PyObject_GetAttrString(value, "value"));
        }

        if (PyFloat_Check(value)) {
            return PyFloat_AsDouble(value);
        }

        if (auto s = PyUnicode_AsUTF8(value)) {
            return s;
        }

        if (value == Py_True) {
            return static_cast<uint64_t>(1);
        }

        if (value == Py_False) {
            return static_cast<uint64_t>(0);
        }

        throw pybind11::type_error(
            stx::format("Conversion error: Got {}, which was not recognized as a valid value type.",
                std::string(py::repr(value))));
    }

    std::vector<Any> valuesFromPyArray(PyObject* value) {
        const auto len = PySequence_Size(value);
        if (len == -1)
            return {};

        std::vector<Any> result;
        result.reserve(len);

        for (Py_ssize_t i = 0; i < len; ++i) {
            auto* item = PySequence_GetItem(value, i);
            result.push_back(valueFromPyObject(item));
        }

        return {std::move(result)};
    }
}

void PyOpenApiClient::bind(py::module_& m) {
    auto serviceClient = py::class_<PyOpenApiClient>(m, "OAClient")
        .def(py::init<std::string, bool, httpcl::Config, std::optional<std::string>, std::optional<std::string>, std::optional<uint32_t>>(),
            "url"_a, "is_local_file"_a = false, "config"_a = httpcl::Config(),
            "api_key"_a = std::optional<std::string>(), "bearer"_a = std::optional<std::string>(), "server_index"_a = std::optional<uint32_t>())
        // zserio >= 2.3.0
        .def("call_method", &PyOpenApiClient::callMethod,
            "method_name"_a, "request"_a, "unused"_a)
        .def("config", [](PyOpenApiClient const& self)->OpenAPIConfig const&{
            return self.client_->config_;
        }, py::return_value_policy::reference_internal);

    py::object serviceClientBase = py::module::import("zserio").attr("ServiceInterface");
    serviceClient.attr("__bases__") = py::make_tuple(serviceClientBase) + serviceClient.attr("__bases__");
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
    auto httpClient = std::make_unique<HttpLibHttpClient>();
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
    if (!request) {
        throw std::runtime_error("The request argument is None!");
    }

    auto response = client_->call(methodName, [&](const std::string& parameter, const std::string& field, ParameterValueHelper& helper)
    {
        if (field == ZSERIO_REQUEST_PART_WHOLE) {
            auto requestData = request.attr("byte_array");
            py::buffer_info info(py::buffer(requestData).request());
            auto* data = reinterpret_cast<uint8_t*>(info.ptr);
            auto length = static_cast<size_t>(info.size);
            return helper.binary(std::vector<uint8_t>(data, data + length));
        }

        auto parts = stx::split<std::vector<std::string>>(field, ".");
        auto currentField = parts.begin();
        auto value = request.attr("zserio_object").cast<py::object>();

        while (currentField != parts.end()) {
            auto internalFieldName = stx::format("_{}_", *currentField);
            if (!py::hasattr(value, internalFieldName.c_str())) {
                throw std::runtime_error(stx::format("Could not find request field {} in method {}.",
                    stx::join(parts.begin(), currentField + 1, "."),
                    methodName));
            }
            value = value.attr(internalFieldName.c_str());
            assert(value);
            ++currentField;
        }

        if (PySequence_Check(value.ptr())) {
            return helper.array(valuesFromPyArray(value.ptr()));
        }

        return helper.value(valueFromPyObject(value.ptr()));
    });

    std::vector<uint8_t> responseData;
    responseData.assign(response.begin(), response.end());
    return responseData;
}
