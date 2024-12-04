#include "http-client.hpp"
#include "uri.hpp"
#include "http-settings.hpp"
#include <httplib.h>

namespace httpcl {

// Forward declarations of helper functions
namespace {
    IHttpClient::Result makeResult(httplib::Result&& result);
    void applyQuery(URIComponents& uri, const Config& config);
    std::unique_ptr<httplib::Client> makeClientAndApplyQuery(
        URIComponents& uri,
        const Config& config,
        time_t const& timeoutSecs,
        bool const& sslCertStrict);
}

// Implementation of helper functions
namespace {

IHttpClient::Result makeResult(httplib::Result&& result) {
    if (result)
        return {result->status, std::move(result->body)};
    return {0, {}};
}

void applyQuery(URIComponents& uri, const Config& config) {
    for (auto const& [key, value] : config.query)
        uri.addQuery(key, value);
}

std::unique_ptr<httplib::Client> makeClientAndApplyQuery(
    URIComponents& uri,
    const Config& config,
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
    if (log().should_log(spdlog::level::debug)) {
        log().debug("  ... full URI: {}", uri.build());
    }
    return client;
}

} // anonymous namespace

// Class implementations
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

IHttpClient::Result HttpLibHttpClient::get(const std::string& uriStr,
                                         const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Get(uri.buildPath().c_str()));
}

IHttpClient::Result HttpLibHttpClient::post(const std::string& uriStr,
                                          const OptionalBodyAndContentType& body,
                                          const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    const std::string& bodyStr = body ? body->body : std::string();
    const char* contentType = body ? body->contentType.c_str() : nullptr;
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Post(uri.buildPath().c_str(), bodyStr, contentType));
}

IHttpClient::Result HttpLibHttpClient::put(const std::string& uriStr,
                                         const OptionalBodyAndContentType& body,
                                         const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    const std::string& bodyStr = body ? body->body : std::string();
    const char* contentType = body ? body->contentType.c_str() : nullptr;
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Put(uri.buildPath().c_str(), bodyStr, contentType));
}

IHttpClient::Result HttpLibHttpClient::del(const std::string& uriStr,
                                         const OptionalBodyAndContentType& body,
                                         const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    const std::string& bodyStr = body ? body->body : std::string();
    const char* contentType = body ? body->contentType.c_str() : nullptr;
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Delete(uri.buildPath().c_str(), bodyStr, contentType));
}

IHttpClient::Result HttpLibHttpClient::patch(const std::string& uriStr,
                                           const OptionalBodyAndContentType& body,
                                           const Config& config)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    const std::string& bodyStr = body ? body->body : std::string();
    const char* contentType = body ? body->contentType.c_str() : nullptr;
    return makeResult(
        makeClientAndApplyQuery(uri, config, timeoutSecs_, sslCertStrict_)
            ->Patch(uri.buildPath().c_str(), bodyStr, contentType));
}

IHttpClient::Result MockHttpClient::get(const std::string& uri,
                                      const Config& config)
{
    auto uriWithQuery = URIComponents::fromStrRfc3986(uri);
    applyQuery(uriWithQuery, config);
    if (getFun)
        return getFun(uriWithQuery.build());
    return {0, ""};
}

IHttpClient::Result MockHttpClient::post(const std::string& uri,
                                       const OptionalBodyAndContentType& body,
                                       const Config& config)
{
    auto uriWithQuery = URIComponents::fromStrRfc3986(uri);
    applyQuery(uriWithQuery, config);
    if (postFun)
        return postFun(uriWithQuery.build(), body, config);
    return {0, ""};
}

IHttpClient::Result MockHttpClient::put(const std::string& uri,
                                      const OptionalBodyAndContentType& body,
                                      const Config& config)
{
    return {0, ""};
}

IHttpClient::Result MockHttpClient::del(const std::string& uri,
                                      const OptionalBodyAndContentType& body,
                                      const Config& config)
{
    return {0, ""};
}

IHttpClient::Result MockHttpClient::patch(const std::string& uri,
                                        const OptionalBodyAndContentType& body,
                                        const Config& config)
{
    return {0, ""};
}

} // namespace httpcl
