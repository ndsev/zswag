#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>

#include "http-settings.hpp"
#include "uri.hpp"

namespace httpcl
{

struct BodyAndContentType {
    std::string body;
    std::string contentType;
};

using OptionalBodyAndContentType = std::optional<BodyAndContentType>;

class IHttpClient
{
public:

    struct Result {
        int status;
        std::string content;
    };

    struct Error : std::runtime_error {
        Result result;

        Error(Result result, std::string const& message)
            : std::runtime_error(message)
            , result(std::move(result))
        {}
    };

    virtual ~IHttpClient() = default;

    virtual Result get(const std::string& path,
                       const Config& config) = 0;
    virtual Result post(const std::string& path,
                        const OptionalBodyAndContentType& body,
                        const Config& config) = 0;
    virtual Result put(const std::string& path,
                       const OptionalBodyAndContentType& body,
                       const Config& config) = 0;
    virtual Result del(const std::string& path,
                       const OptionalBodyAndContentType& body,
                       const Config& config) = 0;
    virtual Result patch(const std::string& path,
                         const OptionalBodyAndContentType& body,
                         const Config& config) = 0;
};

class HttpLibHttpClient : public IHttpClient
{
public:
    explicit HttpLibHttpClient(Config const& config={});
    ~HttpLibHttpClient() override;

    Result get(const std::string& uri,
               const Config& config) override;
    Result post(const std::string& uri,
                const OptionalBodyAndContentType& body,
                const Config& config) override;
    Result put(const std::string& uri,
               const OptionalBodyAndContentType& body,
               const Config& config) override;
    Result del(const std::string& uri,
               const OptionalBodyAndContentType& body,
               const Config& config) override;
    Result patch(const std::string& uri,
                 const OptionalBodyAndContentType& body,
                 const Config& config) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class MockHttpClient : public IHttpClient
{
public:
    std::function<
        IHttpClient::Result(std::string_view /* uri */)
    > getFun;
    std::function<
        IHttpClient::Result(
            std::string_view /* uri */,
            OptionalBodyAndContentType const& /* body */
    )> postFun;

    Result get(const std::string& uri,
               const Config& config) override;
    Result post(const std::string& uri,
                const OptionalBodyAndContentType& body,
                const Config& config) override;
    Result put(const std::string& uri,
               const OptionalBodyAndContentType& body,
               const Config& config) override;
    Result del(const std::string& uri,
               const OptionalBodyAndContentType& body,
               const Config& config) override;
    Result patch(const std::string& uri,
                 const OptionalBodyAndContentType& body,
                 const Config& config) override;
};

}
