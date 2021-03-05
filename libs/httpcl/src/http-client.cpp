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
    httpcl::HTTPSettings settings;

    auto makeClient(const URIComponents& uri)
    {
        auto client = std::make_unique<httplib::Client>(uri.buildHost().c_str());
        client->enable_server_certificate_verification(false);
        settings.apply(uri.build(), *client);

        return client;
    }
};

HttpLibHttpClient::HttpLibHttpClient()
    : impl_(std::make_unique<Impl>())
{}

HttpLibHttpClient::~HttpLibHttpClient()
{}

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
