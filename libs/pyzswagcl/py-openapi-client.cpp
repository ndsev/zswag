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
            auto i64 = PyLong_AsLongLong(value);
            if (i64 == -1 && PyErr_Occurred()) {
                return PyLong_AsUnsignedLongLong(value);
            }
            return i64;
        }

        if (PyFloat_Check(value)) {
            return PyFloat_AsDouble(value);
        }

        if (auto s = PyUnicode_AsUTF8(value)) {
            return s;
        }

        if (value == Py_True) {
            return 1ll;
        }

        if (value == Py_False) {
            return 0ll;
        }

        throw std::runtime_error(
            stx::format("Conversion error: Got {}, which was not recognized as a valid value type.",
                PyUnicode_AsUTF8(PyObject_Str(PyObject_Type(value)))));
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
    auto serviceClient = py::class_<PyOpenApiClient>(m, "Client")
        .def(py::init<std::string, bool>(), "url"_a, "is_local_file"_a = false)
        .def("callMethod", &PyOpenApiClient::callMethod,
             "methodName"_a, "requestData"_a, "context"_a);

    py::object serviceClientBase = py::module::import("zserio").attr("ServiceInterface");
    serviceClient.attr("__bases__") = py::make_tuple(serviceClientBase) + serviceClient.attr("__bases__");
}

PyOpenApiClient::PyOpenApiClient(std::string const& openApiUrl, bool isLocalFile)
{
    auto httpClient = std::make_unique<HttpLibHttpClient>();
    OpenAPIConfig openApiConfig = [&](){
        if (isLocalFile) {
            std::ifstream fs;
            fs.open(openApiUrl);
            return parseOpenAPIConfig(fs);
        }
        else
            return fetchOpenAPIConfig(openApiUrl, *httpClient);
    }();

    client_ = std::make_unique<OpenAPIClient>(openApiConfig, std::move(httpClient));
}

std::vector<uint8_t> PyOpenApiClient::callMethod(
        const std::string& methodName,
        py::bytearray& requestData,
        py::handle context)
{
    if (!context) {
        throw std::runtime_error(stx::format(
            "Unset context argument for call to {}! Please pass the request also as context.",
            methodName));
    }

    auto response = client_->call(methodName, [&](const std::string& parameter, const std::string& field, ParameterValueHelper& helper)
    {
        if (field == ZSERIO_REQUEST_PART_WHOLE) {
            py::buffer_info info(py::buffer(requestData).request());
            auto* data = reinterpret_cast<uint8_t*>(info.ptr);
            auto length = static_cast<size_t>(info.size);
            return helper.binary(std::vector<uint8_t>(data, data + length));
        }

        auto parts = stx::split<std::vector<std::string>>(field, ".");
        auto currentField = parts.begin();
        auto value = context.ptr();

        while (currentField != parts.end()) {
            auto internalFieldName = stx::format("_{}_", *currentField);
            if (!PyObject_HasAttrString(value, internalFieldName.c_str())) {
                throw std::runtime_error(stx::format("Could not find request field {} in method {}.",
                    stx::join(parts.begin(), currentField + 1, "."),
                    methodName));
            }
            value = PyObject_GetAttrString(value, internalFieldName.c_str());
            assert(value);
            ++currentField;
        }

        if (PySequence_Check(value)) {
            return helper.array(valuesFromPyArray(value));
        }

        return helper.value(valueFromPyObject(value));
    });

    std::vector<uint8_t> responseData;
    responseData.assign(response.begin(), response.end());
    return responseData;
}
