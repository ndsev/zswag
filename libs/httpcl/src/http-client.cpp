#include "http-client.hpp"
#include "uri.hpp"

#include <curl/curl.h>

#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace
{

std::once_flag gCurlGlobalInit;

void ensureCurlGlobalInit()
{
    std::call_once(gCurlGlobalInit, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

void applyQuery(httpcl::URIComponents& uri, httpcl::Config const& config)
{
    for (auto const& [key, value] : config.query)
        uri.addQuery(key, value);
}

std::string base64Encode(std::string const& input)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < input.size()) {
        const auto a = static_cast<unsigned char>(input[i++]);
        const auto b = static_cast<unsigned char>(input[i++]);
        const auto c = static_cast<unsigned char>(input[i++]);
        out.push_back(alphabet[a >> 2]);
        out.push_back(alphabet[((a & 0x03) << 4) | (b >> 4)]);
        out.push_back(alphabet[((b & 0x0f) << 2) | (c >> 6)]);
        out.push_back(alphabet[c & 0x3f]);
    }

    if (i < input.size()) {
        const auto a = static_cast<unsigned char>(input[i++]);
        out.push_back(alphabet[a >> 2]);
        if (i < input.size()) {
            const auto b = static_cast<unsigned char>(input[i]);
            out.push_back(alphabet[((a & 0x03) << 4) | (b >> 4)]);
            out.push_back(alphabet[(b & 0x0f) << 2]);
            out.push_back('=');
        } else {
            out.push_back(alphabet[(a & 0x03) << 4]);
            out.push_back('=');
            out.push_back('=');
        }
    }

    return out;
}

size_t writeBody(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* body = static_cast<std::string*>(userdata);
    const auto byteCount = size * nmemb;
    body->append(ptr, byteCount);
    return byteCount;
}

struct CurlSlist {
    ~CurlSlist()
    {
        if (headers)
            curl_slist_free_all(headers);
    }

    void append(std::string const& header)
    {
        headers = curl_slist_append(headers, header.c_str());
    }

    curl_slist* headers = nullptr;
};

struct Transfer {
    std::string method;
    std::string uri;
    httpcl::OptionalBodyAndContentType body;
    httpcl::Config config;
    bool sslCertStrict = false;
    time_t timeoutSecs = 60;

    CURL* easy = nullptr;
    CurlSlist headers;
    std::string response;
    std::string proxy;
    std::string proxyPassword;
    std::string proxyUserPwd;
    std::string bodyBuffer;
    char errorBuffer[CURL_ERROR_SIZE] = {};
    std::promise<httpcl::IHttpClient::Result> result;
};

void addConfiguredHeaders(Transfer& transfer)
{
    for (auto const& [name, value] : transfer.config.headers)
        transfer.headers.append(name + ": " + value);

    std::string cookieHeaderValue;
    for (const auto& cookie : transfer.config.cookies) {
        if (!cookieHeaderValue.empty())
            cookieHeaderValue += "; ";
        cookieHeaderValue += cookie.first + "=" + cookie.second;
    }
    if (!cookieHeaderValue.empty())
        transfer.headers.append("Cookie: " + cookieHeaderValue);

    if (transfer.config.auth) {
        auto password = transfer.config.auth->password;
        if (!transfer.config.auth->keychain.empty())
            password = httpcl::secret::load(
                transfer.config.auth->keychain,
                transfer.config.auth->user);

        transfer.headers.append(
            "Authorization: Basic " +
            base64Encode(transfer.config.auth->user + ":" + password));
    }

    if (transfer.body)
        transfer.headers.append("Content-Type: " + transfer.body->contentType);
}

void configureEasyHandle(Transfer& transfer)
{
    auto uri = httpcl::URIComponents::fromStrRfc3986(transfer.uri);
    applyQuery(uri, transfer.config);
    transfer.uri = uri.build();

    if (httpcl::log().should_log(spdlog::level::debug))
        httpcl::log().debug("  ... full URI: {}", transfer.uri);

    transfer.easy = curl_easy_init();
    if (!transfer.easy)
        throw std::runtime_error("curl_easy_init failed");

    curl_easy_setopt(transfer.easy, CURLOPT_URL, transfer.uri.c_str());
    curl_easy_setopt(transfer.easy, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(transfer.easy, CURLOPT_WRITEDATA, &transfer.response);
    curl_easy_setopt(transfer.easy, CURLOPT_ERRORBUFFER, transfer.errorBuffer);
    curl_easy_setopt(transfer.easy, CURLOPT_PRIVATE, &transfer);
    curl_easy_setopt(transfer.easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(transfer.easy, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(transfer.easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(transfer.easy, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(transfer.easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(transfer.easy, CURLOPT_CONNECTTIMEOUT, static_cast<long>(transfer.timeoutSecs));
    curl_easy_setopt(transfer.easy, CURLOPT_TIMEOUT, static_cast<long>(transfer.timeoutSecs));
    curl_easy_setopt(transfer.easy, CURLOPT_SSL_VERIFYPEER, transfer.sslCertStrict ? 1L : 0L);
    curl_easy_setopt(transfer.easy, CURLOPT_SSL_VERIFYHOST, transfer.sslCertStrict ? 2L : 0L);

    addConfiguredHeaders(transfer);
    if (transfer.headers.headers)
        curl_easy_setopt(transfer.easy, CURLOPT_HTTPHEADER, transfer.headers.headers);

    if (transfer.config.proxy) {
        transfer.proxy = transfer.config.proxy->host + ":" + std::to_string(transfer.config.proxy->port);
        curl_easy_setopt(transfer.easy, CURLOPT_PROXY, transfer.proxy.c_str());

        transfer.proxyPassword = transfer.config.proxy->password;
        if (!transfer.config.proxy->keychain.empty())
            transfer.proxyPassword = httpcl::secret::load(
                transfer.config.proxy->keychain,
                transfer.config.proxy->user);

        if (!transfer.config.proxy->user.empty()) {
            transfer.proxyUserPwd = transfer.config.proxy->user + ":" + transfer.proxyPassword;
            curl_easy_setopt(transfer.easy, CURLOPT_PROXYUSERPWD, transfer.proxyUserPwd.c_str());
        }
    }

    if (transfer.body)
        transfer.bodyBuffer = transfer.body->body;

    if (transfer.method == "GET") {
        curl_easy_setopt(transfer.easy, CURLOPT_HTTPGET, 1L);
    } else if (transfer.method == "POST") {
        curl_easy_setopt(transfer.easy, CURLOPT_POST, 1L);
        curl_easy_setopt(transfer.easy, CURLOPT_POSTFIELDS, transfer.bodyBuffer.data());
        curl_easy_setopt(transfer.easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(transfer.bodyBuffer.size()));
    } else {
        curl_easy_setopt(transfer.easy, CURLOPT_CUSTOMREQUEST, transfer.method.c_str());
        if (transfer.body) {
            curl_easy_setopt(transfer.easy, CURLOPT_POSTFIELDS, transfer.bodyBuffer.data());
            curl_easy_setopt(transfer.easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(transfer.bodyBuffer.size()));
        }
    }
}

} // namespace

namespace httpcl
{

struct CurlHttpClient::Impl {
    Impl()
        : multi(curl_multi_init())
    {
        if (!multi)
            throw std::runtime_error("curl_multi_init failed");

        curl_multi_setopt(multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
        worker = std::thread([this] { workerLoop(); });
    }

    ~Impl()
    {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        cv.notify_all();
        if (multi)
            curl_multi_wakeup(multi);
        if (worker.joinable())
            worker.join();

        if (multi)
            curl_multi_cleanup(multi);
    }

    IHttpClient::Result enqueue(std::shared_ptr<Transfer> transfer)
    {
        auto future = transfer->result.get_future();
        {
            std::lock_guard lock(mutex);
            if (stopping)
                return {0, "CurlHttpClient is stopping"};
            pending.push_back(std::move(transfer));
        }
        cv.notify_all();
        curl_multi_wakeup(multi);
        return future.get();
    }

    void addPendingTransfers()
    {
        std::deque<std::shared_ptr<Transfer>> toAdd;
        {
            std::lock_guard lock(mutex);
            toAdd.swap(pending);
        }

        for (auto& transfer : toAdd) {
            try {
                configureEasyHandle(*transfer);
                curl_multi_add_handle(multi, transfer->easy);
                active[transfer->easy] = std::move(transfer);
            }
            catch (std::exception const& e) {
                transfer->result.set_value({0, e.what()});
                if (transfer->easy)
                    curl_easy_cleanup(transfer->easy);
            }
        }
    }

    void completeFinishedTransfers()
    {
        int messageCount = 0;
        while (auto* message = curl_multi_info_read(multi, &messageCount)) {
            if (message->msg != CURLMSG_DONE)
                continue;

            auto easy = message->easy_handle;
            auto iter = active.find(easy);
            if (iter == active.end())
                continue;

            auto transfer = std::move(iter->second);
            active.erase(iter);

            long responseCode = 0;
            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responseCode);
            curl_multi_remove_handle(multi, easy);
            curl_easy_cleanup(easy);
            transfer->easy = nullptr;

            if (message->data.result == CURLE_OK) {
                transfer->result.set_value({
                    static_cast<int>(responseCode),
                    std::move(transfer->response)});
            } else {
                const auto* error = transfer->errorBuffer[0]
                    ? transfer->errorBuffer
                    : curl_easy_strerror(message->data.result);
                transfer->result.set_value({0, error});
            }
        }
    }

    void failAllActive(std::string const& message)
    {
        for (auto& [easy, transfer] : active) {
            curl_multi_remove_handle(multi, easy);
            curl_easy_cleanup(easy);
            transfer->result.set_value({0, message});
        }
        active.clear();

        std::deque<std::shared_ptr<Transfer>> toFail;
        {
            std::lock_guard lock(mutex);
            toFail.swap(pending);
        }
        for (auto& transfer : toFail)
            transfer->result.set_value({0, message});
    }

    void workerLoop()
    {
        while (true) {
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, [this] {
                    return stopping || !pending.empty() || !active.empty();
                });
                if (stopping && pending.empty() && active.empty())
                    break;
            }

            addPendingTransfers();

            int runningHandles = 0;
            auto multiCode = curl_multi_perform(multi, &runningHandles);
            if (multiCode != CURLM_OK) {
                failAllActive(curl_multi_strerror(multiCode));
                continue;
            }

            completeFinishedTransfers();

            if (!active.empty()) {
                int eventCount = 0;
                curl_multi_poll(multi, nullptr, 0, 100, &eventCount);
            }
        }
    }

    CURLM* multi = nullptr;
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::shared_ptr<Transfer>> pending;
    std::unordered_map<CURL*, std::shared_ptr<Transfer>> active;
    bool stopping = false;
    std::thread worker;
};

using Result = CurlHttpClient::Result;

CurlHttpClient::CurlHttpClient()
{
    ensureCurlGlobalInit();

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

    impl_ = std::make_unique<Impl>();
}

CurlHttpClient::~CurlHttpClient() = default;

Result CurlHttpClient::request(const char* method,
                               const std::string& uri,
                               const std::optional<BodyAndContentType>& body,
                               const Config& config)
{
    auto transfer = std::make_shared<Transfer>();
    transfer->method = method;
    transfer->uri = uri;
    transfer->body = body;
    transfer->config = config;
    transfer->timeoutSecs = timeoutSecs_;
    transfer->sslCertStrict = sslCertStrict_;
    return impl_->enqueue(std::move(transfer));
}

Result CurlHttpClient::get(const std::string& uri,
                           const Config& config)
{
    return request("GET", uri, std::nullopt, config);
}

Result CurlHttpClient::post(const std::string& uri,
                            const std::optional<BodyAndContentType>& body,
                            const Config& config)
{
    return request("POST", uri, body, config);
}

Result CurlHttpClient::put(const std::string& uri,
                           const std::optional<BodyAndContentType>& body,
                           const Config& config)
{
    return request("PUT", uri, body, config);
}

Result CurlHttpClient::del(const std::string& uri,
                           const std::optional<BodyAndContentType>& body,
                           const Config& config)
{
    return request("DELETE", uri, body, config);
}

Result CurlHttpClient::patch(const std::string& uri,
                             const std::optional<BodyAndContentType>& body,
                             const Config& config)
{
    return request("PATCH", uri, body, config);
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

} // namespace httpcl
