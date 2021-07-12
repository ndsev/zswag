#include "openapi-client.hpp"

#include <cassert>
#include <variant>

#include "stx/format.h"

namespace zswagcl
{

template <class _Fun>
std::string replaceTemplate(std::string str,
                            _Fun paramCb)
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
        auto replacement = paramCb(std::string_view(str).substr(begin + 1, len - 1));

        pos = begin + replacement.size();
        str.replace(begin, len + 1, std::move(replacement));
    }

    return str;
}

template <class _Fun>
std::string resolvePath(const OpenAPIConfig::Path& path,
                        const _Fun paramCb)
{
    return replaceTemplate(path.path, [&](std::string_view ident) -> std::string {
        auto parameterIter = path.parameters.find(std::string(ident));
        if (parameterIter == path.parameters.end())
            throw std::runtime_error(stx::format("Could not find path parameter for name '{}' (path: '{}')", ident, path.path));

        const auto& parameter = parameterIter->second;

        ParameterValueHelper helper(parameter);
        auto value = paramCb(parameter.ident, parameter.field, helper);

        return value.pathStr(parameter);
    });
}

template <class _Fun>
void resolveHeaderAndQueryParameters(httpcl::Config& result,
                                     const OpenAPIConfig::Path& path,
                                     const _Fun paramCb)
{
    for (const auto& [key, parameter] : path.parameters) {
        switch (parameter.location) {
        case OpenAPIConfig::Parameter::Location::Query:
        case OpenAPIConfig::Parameter::Location::Header:
        {
            ParameterValueHelper helper(parameter);
            auto values = paramCb(parameter.ident, parameter.field, helper).queryOrHeaderPairs(parameter);
            auto& destination = (parameter.location == OpenAPIConfig::Parameter::Location::Header) ?
                result.headers : result.query;
            for (auto const& value : values)
                destination.insert(value);
            break;
        }
        default:
            break;
        }
    }
}

OpenAPIClient::OpenAPIClient(OpenAPIConfig config,
                             std::unique_ptr<httpcl::IHttpClient> client)
    : config_(std::move(config))
    , client_(std::move(client))
{
    assert(client_);
}

OpenAPIClient::~OpenAPIClient()
{}

std::string OpenAPIClient::call(const std::string& methodIdent,
                                const std::function<ParameterValue(const std::string&, /* parameter ident */
                                                                   const std::string&, /* zserio member path */
                                                                   ParameterValueHelper&)>& paramCb)
{
    auto methodIter = config_.methodPath.find(methodIdent);
    if (methodIter == config_.methodPath.end())
        throw std::runtime_error(stx::format("The method '{}' is not part of the used OpenAPI specification", methodIdent));

    const auto& method = methodIter->second;

    httpcl::URIComponents uri(config_.uri);
    uri.appendPath(resolvePath(method, paramCb));

    auto httpConfig = settings_[uri.build()];
    resolveHeaderAndQueryParameters(httpConfig, method, paramCb);

    const auto& httpMethod = method.httpMethod;
    auto result = ([&]() {
        if (httpMethod == "GET") {
            return  client_->get(uri.build(), httpConfig);
        } else {
            httpcl::OptionalBodyAndContentType body;
            if (method.bodyRequestObject) {
                body = httpcl::BodyAndContentType{
                    "", ZSERIO_OBJECT_CONTENT_TYPE
                };

                OpenAPIConfig::Parameter bodyParameter;
                bodyParameter.ident = "body";
                bodyParameter.format = OpenAPIConfig::Parameter::Format::Binary;

                ParameterValueHelper bodyHelper(bodyParameter);
                body->body = paramCb("", ZSERIO_REQUEST_PART_WHOLE, bodyHelper).bodyStr();
            }

            if (httpMethod == "POST")
                return client_->post(uri.build(), body, httpConfig);
            if (httpMethod == "PUT")
                return client_->put(uri.build(), body, httpConfig);
            if (httpMethod == "PATCH")
                return client_->patch(uri.build(), body, httpConfig);
            if (httpMethod == "DELETE")
                return client_->del(uri.build(), body, httpConfig);

            throw std::runtime_error(stx::format(
                "Unsupported HTTP method '{}' (uri: {})",
                httpMethod,
                uri.build()));
        }
    }());

    if (result.status >= 200 && result.status < 300) {
        return std::move(result.content);
    }

    throw httpcl::IHttpClient::Error(result, stx::format(
        "HTTP status code {} (method: {}, path: {}, uri: {})",
        result.status,
        httpMethod,
        uri.buildPath(),
        uri.build()));
}
}
