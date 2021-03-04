#pragma once

#include <string>
#include <memory>
#include <functional>

namespace ndsafw
{

class IHttpClient
{
public:
    struct Result {
        int status;
        std::string content;
    };

    virtual ~IHttpClient() = default;

    virtual Result get(const std::string& path) = 0;
    virtual Result post(const std::string& path,
                        const std::string& body,
                        const std::string& bodyType) = 0;
    virtual Result put(const std::string& path,
                       const std::string& body,
                       const std::string& bodyType) = 0;
    virtual Result del(const std::string& path,
                       const std::string& body,
                       const std::string& bodyType) = 0;
    virtual Result patch(const std::string& path,
                         const std::string& body,
                         const std::string& bodyType) = 0;
};

class HttpLibHttpClient : public IHttpClient
{
public:
    HttpLibHttpClient();
    ~HttpLibHttpClient();

    Result get(const std::string& uri) override;
    Result post(const std::string& uri,
                const std::string& body,
                const std::string& bodyType) override;
    Result put(const std::string& uri,
               const std::string& body,
               const std::string& bodyType) override;
    Result del(const std::string& uri,
               const std::string& body,
               const std::string& bodyType) override;
    Result patch(const std::string& uri,
                 const std::string& body,
                 const std::string& bodyType) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class MockHttpClient : public IHttpClient
{
public:
    std::function<IHttpClient::Result(const std::string&)> getFun;

    Result get(const std::string& uri) override;
    Result post(const std::string& uri,
                const std::string& body,
                const std::string& bodyType) override;
    Result put(const std::string& uri,
               const std::string& body,
               const std::string& bodyType) override;
    Result del(const std::string& uri,
               const std::string& body,
               const std::string& bodyType) override;
    Result patch(const std::string& uri,
                 const std::string& body,
                 const std::string& bodyType) override;
};

}
