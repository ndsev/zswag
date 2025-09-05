#pragma once

#include <memory>
#include <unordered_map>

#include "httpcl/http-client.hpp"
#include "httpcl/http-settings.hpp"
#include "openapi-config.hpp"

namespace zswagcl
{

struct AuthContext
{
    httpcl::IHttpClient& httpClient;
    const std::string& targetResourceUri;
    httpcl::Settings const& httpSettings;
    httpcl::Config& resultHttpConfigWithAuthorization;
};

class ISecurityHandler
{
public:
    virtual ~ISecurityHandler() = default;

    /**
     * Checks if current HTTP settings can satisfy the derived Security scheme handler.
     * Potentially modifies the AuthContext::httpConfig.
     */
    virtual bool satisfy(
        const OpenAPIConfig::SecurityRequirement& req,
        AuthContext const& ctx,
        std::string& mismatchReason) = 0;
};

class AuthRegistry
{
public:
    /**
     * Construct an auth registry. Instantiates and registers handlers for all known security types.
     */
    AuthRegistry();

    /**
     * Try to satisfy an OR of AND-sets. Throws if none can be satisfied.
     */
    void satisfySecurity(
        const OpenAPIConfig::SecurityAlternatives& alts,
        AuthContext const& ctx);

private:
    std::unordered_map<OpenAPIConfig::SecuritySchemeType, std::shared_ptr<ISecurityHandler>>
        handlers_;
};

}  // namespace zswagcl