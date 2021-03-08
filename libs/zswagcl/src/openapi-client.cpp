#include "openapi-client.hpp"

#include <cassert>
#include <variant>

namespace zswagcl
{

template <class _Fun>
std::string replaceTemplate(std::string str,
                            _Fun fun)
{
    auto pos = std::string::size_type(0);
    while (pos != std::string::npos) {
        auto begin = str.find('{', pos);
        if (begin == std::string::npos)
            break;

        auto end = str.find('}', begin);
        if (end == std::string::npos)
            break;

        auto len = end - begin;
        auto replacement = fun(std::string_view(str).substr(begin + 1, len - 1));

        pos = begin + replacement.size();
        str.replace(begin, len + 1, std::move(replacement));
    }

    return str;
}

template <class _Fun>
std::string resolvePath(const OpenAPIConfig::Path& path,
                        const _Fun fun)
{
    return replaceTemplate(path.path, [&](std::string_view ident) -> std::string {
        auto parameterIter = path.parameters.find(std::string(ident));
        if (parameterIter == path.parameters.end())
            throw std::runtime_error(stx::replace_with("Could not find path parameter for name '?'", "?", ident));

        const auto& parameter = parameterIter->second;

        ParameterValueHelper helper(parameter);
        auto value = fun(parameter.ident, parameter.field, helper);

        return value.pathStr(parameter);
    });
}

template <class _Fun>
auto resolveQueryParameters(const OpenAPIConfig::Path& path,
                            const _Fun fun)
{
    std::vector<std::pair<std::string, std::string>> pairs;

    for (const auto& [key, parameter] : path.parameters) {
        if (parameter.location == OpenAPIConfig::Parameter::Location::Query) {
            ParameterValueHelper helper(parameter);
            auto values = fun(parameter.ident, parameter.field, helper).queryPairs(parameter);

            std::copy(values.begin(), values.end(), std::back_inserter(pairs));
        }
    }

    return pairs;
}

OpenAPIClient::OpenAPIClient(OpenAPIConfig config,
                             std::unique_ptr<httpcl::IHttpClient> client)
    : config(std::move(config))
    , client_(std::move(client))
{
    assert(client_);
}

OpenAPIClient::~OpenAPIClient()
{}

std::string OpenAPIClient::call(const std::string& methodIdent,
                                const std::function<ParameterValue(const std::string&, /* parameter ident */
                                                                   const std::string&, /* zserio member path */
                                                                   ParameterValueHelper&)>& fun)
{
    auto methodIter = config.methodPath.find(methodIdent);
    if (methodIter == config.methodPath.end())
        throw std::runtime_error("Could not find method for name");

    const auto& method = methodIter->second;

    httpcl::URIComponents uri(config.uri);
    uri.appendPath(resolvePath(method, fun));

    for (const auto [key, value] : resolveQueryParameters(method, fun))
        uri.addQuery(std::move(key), std::move(value));

    const auto& httpMethod = method.httpMethod;
    auto result = ([&]() {
        if (httpMethod == "GET") {
            return  client_->get(uri.build());
        } else {
            std::string body, bodyType;
            if (method.bodyRequestObject) {
                bodyType = "application/binary";

                OpenAPIConfig::Parameter bodyParameter;
                bodyParameter.ident = "body";
                bodyParameter.format = OpenAPIConfig::Parameter::Format::Binary;

                ParameterValueHelper bodyHelper(bodyParameter);
                body = fun("", "*", bodyHelper).bodyStr();
            }

            if (httpMethod == "POST")
                return client_->post(uri.build(), body, bodyType);
            if (httpMethod == "PUT")
                return client_->put(uri.build(), body, bodyType);
            if (httpMethod == "PATCH")
                return client_->patch(uri.build(), body, bodyType);
            if (httpMethod == "DELETE")
                return client_->del(uri.build(), body, bodyType);

            throw std::runtime_error("Unsupported HTTP type");
        }
    }());

    if (result.status >= 200 && result.status < 300) {
        return std::move(result.content);
    }

    throw std::runtime_error(stx::replace_with("HTTP status code ? (method: ?, path: ?, uri: ?)", "?",
                                               result.status, httpMethod, uri.buildPath(), uri.build()));
}
}
