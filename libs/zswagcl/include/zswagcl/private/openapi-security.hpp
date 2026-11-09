#pragma once

#include <memory>
#include <unordered_map>

#include "httpcl/http-client.hpp"
#include "httpcl/http-settings.hpp"
#include "openapi-config.hpp"

namespace zswagcl
{

/** Borrowed dependencies and a caller-owned request config; valid for one synchronous check. */
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
     * Check credentials and optionally add authorization to the caller's config.
     * OAuth handlers may perform blocking HTTP requests. False reports a mismatch
     * through mismatchReason; neither failure nor exceptions roll back mutations.
     * A successful return means credentials are available, not server acceptance.
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
     * Try an OR of AND-sets: alternatives in order, requiring every scheme within one alternative.
     * An empty alternatives list requires no authentication. Throw runtime_error
     * if none match. All attempts share the same mutable request config, so
     * authorization added by a failed alternative remains for later attempts.
     * Concurrent calls need separate request configs and a concurrent HTTP client.
     */
    void satisfySecurity(
        const OpenAPIConfig::SecurityAlternatives& alts,
        AuthContext const& ctx);

private:
    std::unordered_map<OpenAPIConfig::SecuritySchemeType, std::shared_ptr<ISecurityHandler>>
        handlers_;
};

}  // namespace zswagcl
