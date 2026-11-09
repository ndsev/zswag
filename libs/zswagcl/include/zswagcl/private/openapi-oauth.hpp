#pragma once

#include "openapi-security.hpp"

namespace zswagcl
{

/**
 * Cache bearer tokens for this handler's lifetime. Settings overrides win over the
 * spec. Cache identity uses token URL, client ID, audience and ordered scopes;
 * changing a secret alone does not invalidate an otherwise live token. Refresh
 * and mint hold one exclusive lock, including network waits, across all keys.
 */
class OAuth2ClientCredentialsHandler final : public ISecurityHandler
{
public:
    /** Append a bearer header, or return false with a reason if acquisition fails. */
    bool satisfy(
        const OpenAPIConfig::SecurityRequirement& req,
        AuthContext const& ctx,
        std::string& mismatchReason) override;

private:
    // OAuth2 grant type constants
    static constexpr const char* GRANT_TYPE_CLIENT_CREDENTIALS = "client_credentials";
    static constexpr const char* GRANT_TYPE_REFRESH_TOKEN = "refresh_token";
    
    struct TokenKey
    {
        std::string tokenUrl, clientId, audience, scopeKey;

        bool operator==(const TokenKey& o) const
        {
            return tokenUrl == o.tokenUrl && clientId == o.clientId && audience == o.audience &&
                scopeKey == o.scopeKey;
        }
    };

    struct TokenKeyHash
    {
        size_t operator()(const TokenKey& k) const
        {
            std::hash<std::string> H;
            return H(k.tokenUrl) ^ H(k.clientId) ^ H(k.audience) ^ H(k.scopeKey);
        }
    };

    struct MintedToken
    {
        std::string accessToken;
        std::string refreshToken;  // may be empty (most CC flows don’t return one)
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::shared_mutex m_;
    std::unordered_map<TokenKey, MintedToken, TokenKeyHash> cache_;

    // Use token-URL settings and accept 2xx responses with a non-empty access_token.
    // Missing expires_in defaults to one hour; subtract 30 seconds for early refresh.
    MintedToken requestToken(
        AuthContext const& httpCtx,
        const httpcl::Config::OAuth2& oauthConfig,
        const std::string& resolvedTokenUrl,
        const std::string& grantType,
        const std::vector<std::string>& resolvedScopes = {},
        const std::string& refreshToken = "") const;
};

/**
 * Try configured OAuth2 before fetching the specification itself. A token URL
 * override is required because no OpenAPI security scheme has been parsed yet.
 * A fresh handler is used per call, so this helper has no cross-call token cache.
 * The input config is copied; a successful bearer token is returned for the
 * caller to apply. Disabled/missing configuration or failed acquisition returns
 * nullopt so the caller can continue with its existing authorization.
 */
std::optional<std::string> acquireOAuth2TokenForSpecFetch(
    httpcl::IHttpClient& httpClient,
    httpcl::Config& httpConfig,
    const std::string& specUrl);

}  // namespace zswagcl
