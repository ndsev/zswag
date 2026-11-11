#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <ctime>

#include "http-settings.hpp"
#include "uri.hpp"
#include "log.hpp"

namespace httpcl
{

/** Owned request bytes and the Content-Type header to send with them. */
struct BodyAndContentType
{
    std::string body;
    std::string contentType;
};

using OptionalBodyAndContentType = std::optional<BodyAndContentType>;

/** Synchronous HTTP transport used by OpenAPI discovery, authentication and method calls. */
class IHttpClient
{
public:
    /** HTTP status and owned response bytes; the Curl implementation uses status 0 for transport
     * failure. */
    struct Result
    {
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

/**
 * Blocking caller API backed by one owned curl-multi worker for concurrent transfers.
 * Each request copies its URI, body and config before queueing; keep the client alive
 * until its callers finish. HTTP status interpretation belongs to the OpenAPI layer.
 */
class CurlHttpClient : public IHttpClient
{
public:
    /**
     * Start the worker and read HTTP_TIMEOUT (seconds, default 60) and HTTP_SSL_STRICT.
     * Any non-empty strict flag enables certificate verification, including "0".
     */
    CurlHttpClient();
    ~CurlHttpClient() override;

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

    /** Queue an owned transfer and wait for its result while the worker multiplexes I/O. */
    Result request(
        const char* method,
        const std::string& uri,
        const OptionalBodyAndContentType& body,
        const Config& config);

    time_t timeoutSecs_ = 60;
    bool sslCertStrict_ = false;
    std::unique_ptr<Impl> impl_;
};

/**
 * Callback transport for tests: GET/POST apply configured query parameters and
 * delegate to their hooks. Unset hooks and PUT/DELETE/PATCH return an empty status-0 result.
 */
class MockHttpClient : public IHttpClient
{
public:
    std::function<
        IHttpClient::Result(std::string_view /* uri */)
    > getFun;
    std::function<
        IHttpClient::Result(
            std::string_view /* uri */,
            OptionalBodyAndContentType const& /* body */,
            Config const& config /* config */
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
