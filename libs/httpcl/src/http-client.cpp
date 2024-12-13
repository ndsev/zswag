#include "http-client.hpp"
#include "uri.hpp"

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

auto makeClientAndApplyQuery(
    httpcl::URIComponents& uri,
    httpcl::Config const& config,
    time_t const& timeoutSecs,
    bool const& sslCertStrict)
{
    auto client = std::make_unique<httplib::Client>(uri.buildHost().c_str());
    client->enable_server_certificate_verification(sslCertStrict);
    client->set_connection_timeout(timeoutSecs);
    client->set_read_timeout(timeoutSecs);
    client->set_follow_location(true);
    config.apply(*client);

    applyQuery(uri, config);
    if (httpcl::log().should_log(spdlog::level::debug)) {
        httpcl::log().debug("  ... full URI: {}", uri.build());
    }
    return client;
}

}

namespace httpcl
{

using Result = HttpLibHttpClient::Result;

HttpLibHttpClient::HttpLibHttpClient() {
    if (auto timeoutStr = std::getenv("HTTP_TIMEOUT")) {
        try {
            timeoutSecs_ = std::stoll(timeoutStr);
        }
        catch (std::exception& e) {
            std::cerr << "Could not parse value of HTTP_TIMEOUT." << std::endl;
        }
    }
    if (auto sslStrictFlagStr = std::getenv("HTTP_SSL_STRICT"))
        sslCertStrict_ = !std::string(sslStrictFlagStr).empty();
}

Result HttpLibHttpClient::get(const std::string& uriStr,
                              const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Get(uri.buildPath()));
}

Result HttpLibHttpClient::post(const std::string& uriStr,
                               const std::optional<BodyAndContentType>& body,
                               const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Post(
                uri.buildPath(),
                body ? body->body : std::string(),
                body ? body->contentType : std::string()));
}

Result HttpLibHttpClient::put(const std::string& uriStr,
                              const std::optional<BodyAndContentType>& body,
                              const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Put(
                uri.buildPath(),
                body ? body->body : std::string(),
                body ? body->contentType : std::string()));
}

Result HttpLibHttpClient::del(const std::string& uriStr,
                              const std::optional<BodyAndContentType>& body,
                              const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Delete(
                uri.buildPath(),
                body ? body->body : std::string(),
                body ? body->contentType : std::string()));
}

Result HttpLibHttpClient::patch(const std::string& uriStr,
                                const std::optional<BodyAndContentType>& body,
                                const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Patch(
                uri.buildPath(),
                body ? body->body : std::string(),
                body ? body->contentType : std::string()));
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
