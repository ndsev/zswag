#include "private/openapi-client.hpp"

#include <cassert>
#include <variant>
#include <future>

#include "stx/format.h"
#include "spdlog/spdlog.h"
#include "httpcl/log.hpp"

namespace zswagcl
{

namespace {

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
        case OpenAPIConfig::ParameterLocation::Query:
        case OpenAPIConfig::ParameterLocation::Header:
        {
            ParameterValueHelper helper(parameter);
            auto values = paramCb(parameter.ident, parameter.field, helper).queryOrHeaderPairs(parameter);
            auto& destination = (parameter.location == OpenAPIConfig::ParameterLocation::Header) ?
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

void checkSecurityAlternativesAndApplyApiKey(OpenAPIConfig::SecurityAlternatives const& alts, httpcl::Config& conf)
{
    if (alts.empty())
        return; // Nothing to check

    bool anyAlternativeMatched = false;
    std::stringstream error;
    error << "The provided HTTP configuration does not satisfy authentication requirements:\n";

    int i = 0;
    for (auto const& schemeSet : alts)
    {
        bool matched = true;

        for (auto const& scheme : schemeSet) {
            std::string reasonForMismatch;
            if (!scheme->checkOrApply(conf, reasonForMismatch)) {
                error << "  In security configuration " << i << ": " << reasonForMismatch << "\n";
                matched = false;
                break;
            }
        }

        if (matched) {
            anyAlternativeMatched = true;
            break;
        }
        ++i;
    }

    if (!anyAlternativeMatched)
        throw std::runtime_error(error.str());
}

}

OpenAPIClient::OpenAPIClient(OpenAPIConfig config,
                             httpcl::Config httpConfig,
                             std::unique_ptr<httpcl::IHttpClient> client,
                             uint32_t serverIndex)
    : config_(std::move(config))
    , httpConfig_(std::move(httpConfig))
    , client_(std::move(client))
{
    if (serverIndex >= config_.servers.size())
        throw httpcl::logRuntimeError(
            fmt::format(
                "The server index {} is out of bounds (servers.size()={}).",
                serverIndex,
                config_.servers.size()));
    server_ = config_.servers[serverIndex];
    httpcl::log().debug("Instantiating OpenApiClient for node at '{}'", server_.build());
    assert(client_);
}

OpenAPIClient::~OpenAPIClient() = default;

std::string OpenAPIClient::call(const std::string& methodIdent,
                                const std::function<ParameterValue(const std::string&, /* parameter ident */
                                                                   const std::string&, /* zserio member path */
                                                                   ParameterValueHelper&)>& paramCb)
{
    auto methodIter = config_.methodPath.find(methodIdent);
    if (methodIter == config_.methodPath.end())
        throw httpcl::logRuntimeError(stx::format("The method '{}' is not part of the used OpenAPI specification", methodIdent));

    const auto& method = methodIter->second;

    auto uri = server_;
    uri.appendPath(resolvePath(method, paramCb));
    std::string builtUri = uri.build();
    std::string debugContext = stx::format("[{} {}]", method.httpMethod, uri.buildPath());
    httpcl::log().debug("{} Calling endpoint {} ...", debugContext, builtUri);

    // Initialize HTTP config from persistent and ad-hoc values
    auto httpConfig = settings_[builtUri];
    httpConfig |= httpConfig_;

    // Make sure that the server responds with correct content type
    httpConfig.headers.insert({"Accept", ZSERIO_OBJECT_CONTENT_TYPE});

    httpcl::log().debug("{} Resolving query/path parameters ...", debugContext);
    resolveHeaderAndQueryParameters(httpConfig, method, paramCb);

    // Check whether the given config fulfills the required security schemes.
    // Throws if the http config does not fulfill any allowed scheme.
    if (method.security) {
        httpcl::log().debug("{} Checking required security schemes for method ...", debugContext);
        checkSecurityAlternativesAndApplyApiKey(*method.security, httpConfig);
    }
    else {
        httpcl::log().debug("{} Checking default security scheme ...", debugContext);
        checkSecurityAlternativesAndApplyApiKey(config_.defaultSecurityScheme, httpConfig);
    }

    const auto& httpMethod = method.httpMethod;
    std::future<httpcl::IHttpClient::Result> resultFuture = ([&]()
    {
        if (httpMethod == "GET") {
            httpcl::log().debug("{} Executing request ...", debugContext);
            return std::async(std::launch::async, [builtUri, httpConfig, this]{
                return client_->get(builtUri, httpConfig);
            });
        } else {
            httpcl::OptionalBodyAndContentType body;
            if (method.bodyRequestObject) {
                httpcl::log().debug("{} Fetching body request body ...", debugContext);
                body = httpcl::BodyAndContentType{
                    "", ZSERIO_OBJECT_CONTENT_TYPE
                };

                OpenAPIConfig::Parameter bodyParameter;
                bodyParameter.ident = "body";
                bodyParameter.format = OpenAPIConfig::Parameter::Format::Binary;

                ParameterValueHelper bodyHelper(bodyParameter);
                body->body = paramCb("", ZSERIO_REQUEST_PART_WHOLE, bodyHelper).bodyStr();
            }

            httpcl::log().debug("{} Executing request ...", debugContext);
            if (httpMethod == "POST")
                return std::async(std::launch::async, [builtUri, body, httpConfig, this]{
                    return client_->post(builtUri, body, httpConfig);
                });
            if (httpMethod == "PUT")
                return std::async(std::launch::async, [builtUri, body, httpConfig, this]{
                    return client_->put(builtUri, body, httpConfig);
                });
            if (httpMethod == "PATCH")
                return std::async(std::launch::async, [builtUri, body, httpConfig, this]{
                    return client_->patch(builtUri, body, httpConfig);
                });
            if (httpMethod == "DELETE")
                return std::async(std::launch::async, [builtUri, body, httpConfig, this]{
                    return client_->del(builtUri, body, httpConfig);
                });

            throw httpcl::logRuntimeError(stx::format(
                "{} Unsupported HTTP method!", debugContext));
        }
    }());

    // Wait for resultFuture
    while (resultFuture.wait_for(std::chrono::seconds{1}) != std::future_status::ready)
        httpcl::log().debug("{} Waiting for response ...", debugContext);
    auto result = resultFuture.get();
    httpcl::log().debug("{} Response received (code {}, content length {} bytes).", debugContext, result.status, result.content.size());

    if (result.status == 200) {
        return std::move(result.content);
    }

    // Throw due to bad response code
    std::string errorStr = stx::format(
        "{} Got HTTP status: {}",
        debugContext,
        result.status);
    throw httpcl::IHttpClient::Error(result, errorStr);
}
}
