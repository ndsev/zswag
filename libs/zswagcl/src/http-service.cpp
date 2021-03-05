// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
#include "http-service.hpp"

#include "base64.hpp"
#include "openapi-parser.hpp"
#include "httpcl/uri.hpp"

#include "zsr/types.hpp"
#include "zsr/find.hpp"
#include "stx/string.h"

#include <optional>
#include <string>

namespace zswagcl
{

static std::optional<std::string>
binFormatBytes(HTTPService::Config::Parameter::Format format,
               const uint8_t* buffer,
               std::size_t len)
{
    std::string res;

    switch (format) {
    case HTTPService::Config::Parameter::Hex:
        res.reserve(len * 2);
        std::for_each(buffer, buffer + len, [&](uint8_t byte) {
            static const auto alphabet = "0123456789abcdef";
            res.push_back(alphabet[byte >> 4]);
            res.push_back(alphabet[byte & 0xf]);
        });
        break;

    case HTTPService::Config::Parameter::Base64:
        res = base64_encode(buffer, len);
        break;

    case HTTPService::Config::Parameter::Base64url:
        res = base64url_encode(buffer, len);
        break;

    case HTTPService::Config::Parameter::Binary:
    default:
        res.assign(buffer, buffer + len);
        break;
    }

    return res;
}

static std::optional<std::string>
queryMember(const std::string& ident,
            HTTPService::Config::Parameter::Format format,
            const zsr::Introspectable& obj,
            const std::vector<uint8_t>& bin)
{
    auto meta = obj.meta();

    /* Special ident to reference the (serialized) object itself */
    if (ident == "*" || ident == "@") {
        return binFormatBytes(format, bin.data(), bin.size());
    }

    zsr::Variant val;
    if (auto field = zsr::find<zsr::Field>(*meta, ident)) {
        val = field->get(obj);
    } else if (auto fun = zsr::find<zsr::Function>(*meta, ident)) {
        val = fun->call(obj);
    }

    if (!val.empty()) {
        // FIXME: Honor `format` for all types.
        if (auto v = val.get<bool>())
            return std::to_string(*v ? 1 : 0);
        if (auto v = val.get<int64_t>())
            return std::to_string(*v);
        if (auto v = val.get<uint64_t>())
            return std::to_string(*v);
        if (auto v = val.get<double>())
            return std::to_string(*v);
        if (auto v = val.get<std::string>())
            return *v;

        if (auto v = val.get<zsr::Introspectable>()) {
            if (!v->meta() || !v->meta()->write)
                return {};

            zserio::BitStreamWriter writer;
            v->meta()->write(const_cast<zsr::Introspectable&>(*v) /* :( */,
                             writer);

            std::size_t len{};
            auto buffer = writer.getWriteBuffer(len);

            return binFormatBytes(format, buffer, len);
        }
    }

    return {};
}

static std::string
resolvePathParameters(const HTTPService::Config::Path& path,
                      const zsr::Variant& request,
                      const std::vector<uint8_t>& bin)
{
    std::string uri = path.path;

    auto begin = uri.find('{');
    while (begin != std::string::npos) {
        auto end = uri.find('}', begin + 1);
        if (end != std::string::npos) {
            auto token = uri.substr(begin + 1, end - begin - 1);
            if (!token.empty()) {
                const auto& parameter = path.parameters.find(token);
                if (parameter == path.parameters.end())
                    goto skip;

                if (parameter->second.location != HTTPService::Config::Parameter::Path)
                    goto skip;

                std::string val;
                if (auto obj = request.get<zsr::Introspectable>()) {
                    if (auto str = queryMember(parameter->second.field,
                                               parameter->second.format,
                                               *obj,
                                               bin))
                        val = httpcl::URIComponents::encode(*str);
                }

                if (!val.empty()) {
                    uri.replace(begin, end - begin + 1, val);
                    end = begin + val.size();
                }
            }
        } else {
            end = begin + 1;
        }

    skip:
        begin = uri.find('{', end);
    }

    return uri;
}

struct HTTPService::Impl
{
    std::unique_ptr<httpcl::IHttpClient> client;
    HTTPService::Config const cfg;

    Impl(Config cfg, std::unique_ptr<httpcl::IHttpClient> client)
        : client(std::move(client))
        , cfg(std::move(cfg))
    {}
};

HTTPService::HTTPService(const Config& cfg,
                         std::unique_ptr<httpcl::IHttpClient> client)
    : impl(std::make_unique<HTTPService::Impl>(cfg, std::move(client)))
{}

HTTPService::~HTTPService()
{}

void HTTPService::callMethod(const std::string& methodName,
                             const std::vector<uint8_t>& requestData,
                             std::vector<uint8_t>& responseData,
                             void* context)
{
    assert(impl);
    assert(context);

    if (impl) {
        const auto* ctx = reinterpret_cast<const zsr::ServiceMethod::Context*>(context);
        const auto& methods = impl->cfg.methodPath;

        auto methodSpec = methods.find(methodName);
        if (methodSpec != methods.end()) {
            auto httpMethod = methodSpec->second.httpMethod;

            httpcl::URIComponents uri(impl->cfg.uri);
            uri.appendPath(resolvePathParameters(methodSpec->second,
                                                 ctx->request,
                                                 requestData));

            for (const auto& parameter : methodSpec->second.parameters) {
                if (parameter.second.location == HTTPService::Config::Parameter::Query) {
                    if (auto obj = ctx->request.get<zsr::Introspectable>()) {
                        auto queryValue = queryMember(parameter.second.field,
                                                      parameter.second.format,
                                                      *obj,
                                                      requestData).value_or("");

                        uri.addQuery(parameter.first, std::move(queryValue));
                    }
                    else
                        uri.addQuery(parameter.first, parameter.second.defaultValue);
                }
            }

            auto res = [&, this]() {
                if (httpMethod == "GET") {
                    return impl->client->get(uri.build());
                } else {
                    static auto const bodyType = "application/binary";
                    std::string body;
                    if (!requestData.empty())
                        body = std::string((const char *) requestData.data(), requestData.size());

                    if (httpMethod == "POST")
                        return impl->client->post(uri.build(), body, bodyType);
                    else if (httpMethod == "PUT")
                        return impl->client->put(uri.build(), body, bodyType);
                    else if (httpMethod == "PATCH")
                        return impl->client->patch(uri.build(), body, bodyType);
                    else if (httpMethod == "DELETE")
                        return impl->client->del(uri.build(), body, bodyType);
                    else
                        throw std::runtime_error("Unsupported HTTP method");
                }
            }();

            if (res.status >= 200 && res.status < 300) {
                const auto& body = res.content;
                responseData.reserve(body.size());
                std::copy(body.begin(), body.end(), std::back_inserter(responseData));
            }
            else {
                responseData.clear();

                auto info = stx::replace_with("HTTP status code ? (method: ?, path: ?, uri: ?)",
                                              "?",
                                              std::to_string(res.status),
                                              httpMethod,
                                              uri.buildPath(),
                                              uri.build());
                throw std::runtime_error(info.c_str());
            }
        }
    }
}

}
