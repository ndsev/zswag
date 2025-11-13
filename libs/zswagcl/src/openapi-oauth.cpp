#include "private/openapi-oauth.hpp"

#include <stx/format.h>

#include "base64.hpp"
#include "httpcl/oauth1-signature.hpp"

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
            httpcl::log().debug("[OAuth2] Using cached token (still valid)");
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
            httpcl::log().debug("[OAuth2] Cached token expired, attempting refresh...");
            try {
                httpcl::log().debug("Trying token refresh at {} ...", refreshUrl);
                auto newTok = requestToken(ctx, oauthConfig, refreshUrl,
                    GRANT_TYPE_REFRESH_TOKEN, {}, it->second.refreshToken);
                it->second = newTok;
                ctx.resultHttpConfigWithAuthorization.headers.insert({"Authorization", "Bearer " + newTok.accessToken});
                httpcl::log().debug("  ... refresh successful.");
                httpcl::log().debug("[OAuth2] Refreshed access token: {}", newTok.accessToken);
                return true;
            }
            catch (std::exception const& e) {
                httpcl::log().debug("  ... refresh failed with error: {}", e.what());
            }
        }
        else if (it != cache_.end()) {
            httpcl::log().debug("[OAuth2] Cached token expired (no refresh token), minting new...");
        }
        else {
            httpcl::log().debug("[OAuth2] No cached token, minting new...");
        }

        // Mint fresh
        try {
            httpcl::log().debug("Trying token mint at {} ...", tokenUrl);
            auto minted = requestToken(ctx, oauthConfig, tokenUrl,
                GRANT_TYPE_CLIENT_CREDENTIALS, scopes);
            cache_[key] = minted;
            ctx.resultHttpConfigWithAuthorization.headers.insert({"Authorization", "Bearer " + minted.accessToken});
            httpcl::log().debug("  ... mint successful.");
            httpcl::log().debug("[OAuth2] Minted access token: {}", minted.accessToken);
        }
        catch (std::exception const& e) {
            mismatchReason = stx::format("OAuth token mint failed: {}", e.what());
            return false;
        }

        return true;
    }
}

/**
 * Parse URL-encoded body into parameter map.
 */
static std::map<std::string, std::string> parseBodyParams(const std::string& body)
{
    std::map<std::string, std::string> params;
    size_t start = 0;
    while (start < body.length()) {
        size_t amp = body.find('&', start);
        if (amp == std::string::npos) amp = body.length();

        std::string pair = body.substr(start, amp - start);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string value = pair.substr(eq + 1);
            // Decode URL-encoded values
            value = httplib::detail::decode_url(value, false);
            params[key] = value;
        }

        start = amp + 1;
    }
    return params;
}

/**
 * Add client authentication to token request based on configured method.
 */
static void addClientAuthentication(
    httpcl::Config& conf,
    const httpcl::Config::OAuth2& oauthConfig,
    const std::string& tokenUrl,
    const std::string& body,
    std::string& outSecret)
{
    // Load secret from keychain if configured
    outSecret = oauthConfig.clientSecret;
    if (!oauthConfig.clientSecretKeychain.empty()) {
        outSecret = httpcl::secret::load(oauthConfig.clientSecretKeychain, oauthConfig.clientId);
    }

    if (outSecret.empty()) {
        return;  // Public client, no authentication
    }

    auto authMethod = oauthConfig.getTokenEndpointAuthMethod();

    httpcl::log().debug("[OAuth2] Token endpoint auth method: {}",
        authMethod == httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature
        ? "rfc5849-oauth1-signature (HMAC-SHA256)"
        : "rfc6749-client-secret-basic (HTTP Basic)");

    if (authMethod == httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature) {
        // OAuth 1.0 signature-based authentication
        auto bodyParams = parseBodyParams(body);
        int nonceLength = oauthConfig.tokenEndpointAuth
            ? oauthConfig.tokenEndpointAuth->nonceLength
            : 16;

        httpcl::log().debug("[OAuth2] Building OAuth 1.0 signature for token request");
        httpcl::log().trace("[OAuth2]   URL: POST {}", tokenUrl);
        httpcl::log().trace("[OAuth2]   Nonce length: {}", nonceLength);
        httpcl::log().trace("[OAuth2]   Body params count: {}", bodyParams.size());

        std::string authHeader = httpcl::oauth1::buildAuthorizationHeader(
            "POST",
            tokenUrl,
            oauthConfig.clientId,
            outSecret,
            bodyParams,
            nonceLength);

        httpcl::log().trace("[OAuth2]   Authorization header: OAuth ...");

        conf.headers.insert({"Authorization", authHeader});
    }
    else {
        // RFC 6749 HTTP Basic Authentication (default)
        const auto cred = oauthConfig.clientId + ":" + outSecret;
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

    // Add client authentication (Basic or OAuth1 signature)
    std::string secret;
    addClientAuthentication(tokenRequestConf, oauthConfig, resolvedTokenUrl, body, secret);

    // Add client_id for public clients (no secret)
    if (secret.empty()) {
        body += "&client_id=" + httplib::detail::encode_url(oauthConfig.clientId);
    }

    httpcl::log().debug("[OAuth2] Requesting token: grant_type={}, url={}", grantType, resolvedTokenUrl);
    httpcl::log().trace("[OAuth2]   client_id: {}", oauthConfig.clientId);
    if (grantType == GRANT_TYPE_CLIENT_CREDENTIALS && !resolvedScopes.empty()) {
        httpcl::log().trace("[OAuth2]   scopes: {}", stx::join(resolvedScopes.begin(), resolvedScopes.end(), ", "));
    }

    auto res = httpCtx.httpClient.post(
        resolvedTokenUrl,
        httpcl::BodyAndContentType{body, "application/x-www-form-urlencoded"},
        tokenRequestConf);

    httpcl::log().debug("[OAuth2] Token endpoint response: status={}, body_size={}", res.status, res.content.size());

    if (res.status < 200 || res.status >= 300) {
        httpcl::log().warn("[OAuth2] Token request failed with status {}", res.status);
        httpcl::log().trace("[OAuth2]   Error response: {}", res.content);
        throw httpcl::IHttpClient::Error(res,
            stx::format("OAuth2 token endpoint returned non-2xx for grant_type={}.", grantType));
    }

    httpcl::log().trace("[OAuth2]   Success response: {}", res.content);

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

std::optional<std::string> acquireOAuth2TokenForSpecFetch(
    httpcl::IHttpClient& httpClient,
    httpcl::Config& httpConfig,
    const std::string& specUrl)
{
    // Check if OAuth2 is configured
    if (!httpConfig.oauth2) {
        httpcl::log().trace("[OAuth2] No OAuth2 config for spec fetch at {}", specUrl);
        return std::nullopt;
    }

    // Check if useForSpecFetch is enabled
    if (!httpConfig.oauth2->useForSpecFetch) {
        httpcl::log().debug("[OAuth2] useForSpecFetch=false, skipping token acquisition for spec fetch");
        return std::nullopt;
    }

    httpcl::log().debug("[OAuth2] Acquiring token for OpenAPI spec fetch at {}", specUrl);

    // Create minimal security requirement for OAuth2 client credentials
    auto scheme = std::make_shared<OpenAPIConfig::SecurityScheme>();
    scheme->type = SecuritySchemeType::OAuth2ClientCredentials;

    // Use tokenUrlOverride if available, otherwise we'll need a dummy (handler will fail gracefully if missing)
    if (!httpConfig.oauth2->tokenUrlOverride.empty()) {
        scheme->oauthTokenUrl = httpConfig.oauth2->tokenUrlOverride;
    }

    SecurityRequirement req;
    req.scheme = scheme;
    req.scopes = httpConfig.oauth2->scopesOverride;  // Use configured scopes

    // Create auth context with a copy of httpConfig that will be modified by the handler
    httpcl::Config resultConfig = httpConfig;
    httpcl::Settings settings;
    AuthContext ctx{httpClient, specUrl, settings, resultConfig};

    // Try to satisfy the requirement (this will acquire the token)
    OAuth2ClientCredentialsHandler handler;
    std::string mismatchReason;
    if (!handler.satisfy(req, ctx, mismatchReason)) {
        httpcl::log().warn("[OAuth2] Failed to acquire token for spec fetch: {}", mismatchReason);
        return std::nullopt;
    }

    // Extract the token from the Authorization header
    auto authHeader = resultConfig.headers.find("Authorization");
    if (authHeader == resultConfig.headers.end()) {
        httpcl::log().warn("[OAuth2] Token acquired but Authorization header not set");
        return std::nullopt;
    }

    // Extract token from "Bearer <token>" format
    const std::string& headerValue = authHeader->second;
    if (headerValue.size() > 7 && headerValue.substr(0, 7) == "Bearer ") {
        std::string token = headerValue.substr(7);
        httpcl::log().debug("[OAuth2] Successfully acquired token for spec fetch");
        return token;
    }

    httpcl::log().warn("[OAuth2] Authorization header present but not in Bearer format: {}", headerValue);
    return std::nullopt;
}

}  // namespace zswagcl