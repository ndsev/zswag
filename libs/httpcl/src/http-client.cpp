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

}

namespace httpcl
{

using Result = HttpLibHttpClient::Result;

struct HttpLibHttpClient::Impl
{
    Impl(std::map<std::string, std::string> headers) : headers(std::move(headers)) {}

    httpcl::HTTPSettings settings;
    std::map<std::string, std::string> headers;

    auto makeClient(const URIComponents& uri)
    {
        auto client = std::make_unique<httplib::Client>(uri.buildHost().c_str());
        client->enable_server_certificate_verification(false);
        client->set_connection_timeout(60000);
        client->set_read_timeout(60000);
        settings.apply(uri.build(), *client, headers);

        return client;
    }
};

HttpLibHttpClient::HttpLibHttpClient(std::map<std::string, std::string> const& headers)
    : impl_(std::make_unique<Impl>(headers))
{}

HttpLibHttpClient::~HttpLibHttpClient() {
    /* Nontrivial due to impl unique-ptr. */
}

Result HttpLibHttpClient::get(const std::string& uriStr)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClient(uri)->Get(uri.buildPath().c_str()));
}

Result HttpLibHttpClient::post(const std::string& uriStr,
                               const std::string& body,
                               const std::string& bodyType)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClient(uri)->Post(uri.buildPath().c_str(),
                                                   body,
                                                   bodyType.c_str()));
}

Result HttpLibHttpClient::put(const std::string& uriStr,
                              const std::string& body,
                              const std::string& bodyType)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClient(uri)->Put(uri.buildPath().c_str(),
                                                  body,
                                                  bodyType.c_str()));
}

Result HttpLibHttpClient::del(const std::string& uriStr,
                              const std::string& body,
                              const std::string& bodyType)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClient(uri)->Delete(uri.buildPath().c_str(),
                                                     body,
                                                     bodyType.c_str()));
}

Result HttpLibHttpClient::patch(const std::string& uriStr,
                                const std::string& body,
                                const std::string& bodyType)
{
    auto uri = URIComponents::fromStrRfc3986(uriStr);
    return makeResult(impl_->makeClient(uri)->Patch(uri.buildPath().c_str(),
                                                    body,
                                                    bodyType.c_str()));
}

Result MockHttpClient::get(const std::string& uri)
{
    if (getFun)
        return getFun(uri);
    return {0, ""};
}

Result MockHttpClient::post(const std::string& uri,
                            const std::string& body,
                            const std::string& bodyType)
{
    if (postFun)
        return postFun(uri, body, bodyType);
    return {0, ""};
}

Result MockHttpClient::put(const std::string& uri,
                           const std::string& body,
                           const std::string& bodyType)
{
    return {0, ""};
}

Result MockHttpClient::del(const std::string& uri,
                           const std::string& body,
                           const std::string& bodyType)
{
    return {0, ""};
}

Result MockHttpClient::patch(const std::string& uri,
                             const std::string& body,
                             const std::string& bodyType)
{
    return {0, ""};
}

} // namespace ndsafw
