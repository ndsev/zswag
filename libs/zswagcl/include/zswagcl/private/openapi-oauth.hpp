#pragma once

#include "openapi-security.hpp"

namespace zswagcl
{

class OAuth2ClientCredentialsHandler final : public ISecurityHandler
{
public:
    bool satisfy(
        const OpenAPIConfig::SecurityRequirement& req,
        AuthContext& ctx,
        std::string& mismatchReason) override;

private:
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

    MintedToken fetchToken(
        httpcl::IHttpClient& http,
        const httpcl::Config& requestConfForResource,
        const std::string& tokenUrl,
        const httpcl::Config::OAuth2& cc,
        const std::vector<std::string>& scopes);

    MintedToken refreshToken(
        httpcl::IHttpClient& http,
        const httpcl::Config& requestConfForResource,
        const std::string& refreshUrl,
        const httpcl::Config::OAuth2& cc,
        const std::string& refreshToken);
};

}  // namespace zswagcl