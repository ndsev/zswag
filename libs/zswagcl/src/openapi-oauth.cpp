#include "private/openapi-oauth.hpp"

#include <stx/format.h>

#include "base64.hpp"

#include <stx/string.h>
#include "yaml-cpp/yaml.h"

using namespace std::chrono;

namespace zswagcl
{

using SecurityRequirement = OpenAPIConfig::SecurityRequirement;
using SecuritySchemeType = OpenAPIConfig::SecuritySchemeType;

bool OAuth2ClientCredentialsHandler::satisfy(
    const SecurityRequirement& req, AuthContext const& ctx, std::string& mismatchReason)
{
    const auto& scheme = *req.scheme;
    if (scheme.type != SecuritySchemeType::OAuth2ClientCredentials)
        return false;  // registry shouldn't route here otherwise

    if (!ctx.resultHttpConfigWithAuthorization.oauth2) {
        mismatchReason = "OAuth2 client-credentials required but no oauth2 config present in http-settings.";
        return false;
    }
    const auto& oauthConfig = *ctx.resultHttpConfigWithAuthorization.oauth2;

    // Resolve scopes (override > spec)
    std::vector<std::string> scopes = oauthConfig.scopesOverride.empty() ? req.scopes : oauthConfig.scopesOverride;

    // Resolve URLs (override > spec)
    std::string tokenUrl =
        oauthConfig.tokenUrlOverride.empty() ? scheme.oauthTokenUrl : oauthConfig.tokenUrlOverride;
    std::string refreshUrl =
        oauthConfig.refreshUrlOverride.empty() ? scheme.oauthRefreshUrl : oauthConfig.refreshUrlOverride;

    if (tokenUrl.empty()) {
        mismatchReason = "OAuth2 client-credentials: tokenUrl missing (spec/http-settings).";
        return false;
    }
    if (refreshUrl.empty()) {
        refreshUrl = tokenUrl;
    }

    const std::string scopeKey = stx::join(scopes.begin(), scopes.end(), ":");
    TokenKey key{tokenUrl, oauthConfig.clientId, oauthConfig.audience, scopeKey};

    // Try cache
    {
        std::shared_lock lk(m_);
        auto it = cache_.find(key);
        if (it != cache_.end() && steady_clock::now() < it->second.expiresAt) {
            ctx.resultHttpConfigWithAuthorization.headers.insert({"Authorization", "Bearer " + it->second.accessToken});
            return true;
        }
    }

    {
        std::unique_lock lk(m_);
        auto it = cache_.find(key);

        // Check if someone else updated the token before we got the unique lock
        if (it != cache_.end() && steady_clock::now() < it->second.expiresAt) {
            ctx.resultHttpConfigWithAuthorization.headers.insert({"Authorization", "Bearer " + it->second.accessToken});
            return true;
        }

        // Try refresh if we had an entry, but it expired and has refresh token
        if (it != cache_.end() && !it->second.refreshToken.empty()) {
            try {
                httpcl::log().debug("Trying token refresh at {} ...", refreshUrl);
                auto newTok = requestToken(ctx, oauthConfig, refreshUrl, 
                    GRANT_TYPE_REFRESH_TOKEN, {}, it->second.refreshToken);
                it->second = newTok;
                ctx.resultHttpConfigWithAuthorization.headers.insert({"Authorization", "Bearer " + newTok.accessToken});
                httpcl::log().debug("  ... refresh successful.");
                return true;
            }
            catch (std::exception const& e) {
                httpcl::log().debug("  ... refresh failed with error: {}", e.what());
            }
        }

        // Mint fresh
        try {
            httpcl::log().debug("Trying token mint at {} ...", tokenUrl);
            auto minted = requestToken(ctx, oauthConfig, tokenUrl, 
                GRANT_TYPE_CLIENT_CREDENTIALS, scopes);
            cache_[key] = minted;
            ctx.resultHttpConfigWithAuthorization.headers.insert({"Authorization", "Bearer " + minted.accessToken});
            httpcl::log().debug("  ... mint successful.");
        }
        catch (std::exception const& e) {
            mismatchReason = stx::format("OAuth token mint failed: {}", e.what());
            return false;
        }

        return true;
    }
}

static void addClientAuthIfSecretPresent(
    httpcl::Config& conf,
    const httpcl::Config::OAuth2& cc,
    std::string& outSecret)
{
    outSecret = cc.clientSecret;
    if (!cc.clientSecretKeychain.empty()) {
        outSecret = httpcl::secret::load(cc.clientSecretKeychain, cc.clientId);
    }

    if (!outSecret.empty()) {
        const auto cred = cc.clientId + ":" + outSecret;
        const auto b64 = base64_encode(
            reinterpret_cast<const unsigned char*>(cred.data()),
            static_cast<unsigned>(cred.size()));
        conf.headers.insert({"Authorization", "Basic " + b64});
    }
}

OAuth2ClientCredentialsHandler::MintedToken OAuth2ClientCredentialsHandler::requestToken(
    AuthContext const& httpCtx,
    const httpcl::Config::OAuth2& oauthConfig,
    const std::string& resolvedTokenUrl,
    const std::string& grantType,
    const std::vector<std::string>& resolvedScopes,
    const std::string& refreshToken)
{
    auto tokenRequestConf = httpCtx.httpSettings[resolvedTokenUrl];

    std::string secret;
    addClientAuthIfSecretPresent(tokenRequestConf, oauthConfig, secret);

    // Build request body based on grant type
    std::string body = "grant_type=" + grantType;
    
    if (grantType == GRANT_TYPE_CLIENT_CREDENTIALS) {
        if (!resolvedScopes.empty())
            body += "&scope=" + httplib::detail::encode_url(stx::join(resolvedScopes.begin(), resolvedScopes.end(), " "));
        if (!oauthConfig.audience.empty())
            body += "&audience=" + httplib::detail::encode_url(oauthConfig.audience);
    }
    else if (grantType == GRANT_TYPE_REFRESH_TOKEN) {
        body += "&refresh_token=" + httplib::detail::encode_url(refreshToken);
    }
    
    // Add client_id for public clients (no secret)
    if (secret.empty()) {
        body += "&client_id=" + httplib::detail::encode_url(oauthConfig.clientId);
    }

    auto res = httpCtx.httpClient.post(
        resolvedTokenUrl,
        httpcl::BodyAndContentType{body, "application/x-www-form-urlencoded"},
        tokenRequestConf);

    if (res.status < 200 || res.status >= 300) {
        throw httpcl::IHttpClient::Error(res, 
            stx::format("OAuth2 token endpoint returned non-2xx for grant_type={}.", grantType));
    }

    // Parse response - common for both grant types
    auto jsonResult = YAML::Load(res.content);
    MintedToken out;
    if (auto accessTokenNode = jsonResult["access_token"])
        out.accessToken = accessTokenNode.as<std::string>();
    if (out.accessToken.empty()) {
        throw std::runtime_error(
            stx::format("OAuth2: access_token missing in response for grant_type={}.", grantType));
    }

    int expiresIn = 3600;
    if (auto expiresInNode = jsonResult["expires_in"])
        expiresIn = expiresInNode.as<int>();
    // Subtract a thirty-second jiggle period from the token TTL
    out.expiresAt = steady_clock::now() + seconds(expiresIn - 30);

    // Handle refresh token in response
    if (auto refreshTokenNode = jsonResult["refresh_token"])
        out.refreshToken = refreshTokenNode.as<std::string>();
    else if (grantType == GRANT_TYPE_REFRESH_TOKEN && !refreshToken.empty())
        out.refreshToken = refreshToken;  // Keep the old refresh token if not reissued

    return out;
}

}  // namespace zswagcl