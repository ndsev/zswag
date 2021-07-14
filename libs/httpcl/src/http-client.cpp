#include "http-client.hpp"
#include "uri.hpp"

#include "httpcl/http-settings.hpp"

#include <httplib.h>

namespace
{

httpcl::IHttpClient::Result makeResult(httplib::Result&& result)
{
    if (result)
        return {result->status, std::move(result->body)};
    return {0, {}};
}

void applyQuery(httpcl::URIComponents& uri, httpcl::Config const& config) {
    for (auto const& [key, value] : config.query)
        uri.addQuery(key, value);
}

}

namespace httpcl
{

using Result = HttpLibHttpClient::Result;

struct HttpLibHttpClient::Impl
{
    explicit Impl(Config config) : userConfig_(std::move(config)) {}

    Config userConfig_;

    auto makeClientAndApplyQuery(URIComponents& uri, Config const& config)
    {
        auto client = std::make_unique<httplib::Client>(uri.buildHost().c_str());
        client->enable_server_certificate_verification(false);
        client->set_connection_timeout(60000);
        client->set_read_timeout(60000);

        // First fetch persistent config for this URI,
        // then apply user-specified config overrides.
        auto uriSpecificConfig = config;
        uriSpecificConfig |= userConfig_;
        uriSpecificConfig.apply(*client);

        applyQuery(uri, config);

        return client;
    }
};

HttpLibHttpClient::HttpLibHttpClient(Config const& config)
    : impl_(std::make_unique<Impl>(config))
{}

HttpLibHttpClient::~HttpLibHttpClient() {
    /* Nontrivial due to impl unique-ptr. */
}

Result HttpLibHttpClient::get(const std::string& uriStr,
                              const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClientAndApplyQuery(uri, config)->Get(
        uri.buildPath().c_str(),
        {config.headers.begin(), config.headers.end()}));
}

Result HttpLibHttpClient::post(const std::string& uriStr,
                               const std::optional<BodyAndContentType>& body,
                               const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClientAndApplyQuery(uri, config)->Post(
        uri.buildPath().c_str(),
        {config.headers.begin(), config.headers.end()},
        body ? body->body : std::string(),
        body ? body->contentType.c_str() : nullptr));
}

Result HttpLibHttpClient::put(const std::string& uriStr,
                              const std::optional<BodyAndContentType>& body,
                              const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClientAndApplyQuery(uri, config)->Put(
        uri.buildPath().c_str(),
        {config.headers.begin(), config.headers.end()},
        body ? body->body : std::string(),
        body ? body->contentType.c_str() : nullptr));
}

Result HttpLibHttpClient::del(const std::string& uriStr,
                              const std::optional<BodyAndContentType>& body,
                              const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClientAndApplyQuery(uri, config)->Delete(
        uri.buildPath().c_str(),
        {config.headers.begin(), config.headers.end()},
        body ? body->body : std::string(),
        body ? body->contentType.c_str() : nullptr));
}

Result HttpLibHttpClient::patch(const std::string& uriStr,
                                const std::optional<BodyAndContentType>& body,
                                const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClientAndApplyQuery(uri, config)->Patch(
        uri.buildPath().c_str(),
        {config.headers.begin(), config.headers.end()},
        body ? body->body : std::string(),
        body ? body->contentType.c_str() : nullptr));
}

Result MockHttpClient::get(const std::string& uri,
                           const Config& config)
{
    auto uriWithQuery = URIComponents::fromStrRfc3986(uri);
    applyQuery(uriWithQuery, config);
    if (getFun)
        return getFun(uriWithQuery.build());
    return {0, ""};
}

Result MockHttpClient::post(const std::string& uri,
                            const std::optional<BodyAndContentType>& body,
                            const Config& config)
{
    auto uriWithQuery = URIComponents::fromStrRfc3986(uri);
    applyQuery(uriWithQuery, config);
    if (postFun)
        return postFun(uriWithQuery.build(), body, config);
    return {0, ""};
}

Result MockHttpClient::put(const std::string& uri,
                           const std::optional<BodyAndContentType>& body,
                           const Config& config)
{
    return {0, ""};
}

Result MockHttpClient::del(const std::string& uri,
                           const std::optional<BodyAndContentType>& body,
                           const Config& config)
{
    return {0, ""};
}

Result MockHttpClient::patch(const std::string& uri,
                             const std::optional<BodyAndContentType>& body,
                             const Config& config)
{
    return {0, ""};
}

} // namespace ndsafw
