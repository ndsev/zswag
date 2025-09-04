#include "private/openapi-oauth.hpp"

#include <stx/string.h>

using namespace std::chrono;

namespace zswagcl
{

using SecurityRequirement = OpenAPIConfig::SecurityRequirement;
using SecuritySchemeType = OpenAPIConfig::SecuritySchemeType;

bool OAuth2ClientCredentialsHandler::satisfy(
    const SecurityRequirement& req, AuthContext& ctx, std::string& mismatchReason)
{
    const auto& scheme = *req.scheme;
    if (scheme.type != SecuritySchemeType::OAuth2ClientCredentials)
        return false;  // registry shouldn't route here otherwise

    if (!ctx.httpConfig.oauth2) {
        mismatchReason = "OAuth2 client-credentials required but no oauth2 config present in http-settings.";
        return false;
    }
    const auto& oauthConfig = *ctx.httpConfig.oauth2;

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
            ctx.httpConfig.headers.insert({"Authorization", "Bearer " + it->second.accessToken});
            return true;
        }
    }

    {
        std::unique_lock lk(m_);
        auto it = cache_.find(key);

        // Check if someone else updated the token before we got the unique lock
        if (it != cache_.end() && steady_clock::now() < it->second.expiresAt) {
            ctx.httpConfig.headers.insert({"Authorization", "Bearer " + it->second.accessToken});
            return true;
        }

        // Try refresh if we had an entry, but it expired and has refresh token
        if (it != cache_.end() && !it->second.refreshToken.empty()) {
            try {
                auto newTok =
                    refreshToken(ctx.httpClient, ctx.httpConfig, refreshUrl, oauthConfig, it->second.refreshToken);
                it->second = newTok;
                ctx.httpConfig.headers.insert({"Authorization", "Bearer " + newTok.accessToken});
                return true;
            }
            catch (...) {
                // fall through to mint fresh
            }
        }

        // Mint fresh
        auto minted = fetchToken(ctx.httpClient, ctx.httpConfig, tokenUrl, oauthConfig, scopes);
        cache_[key] = minted;
        ctx.httpConfig.headers.insert({"Authorization", "Bearer " + minted.accessToken});
    }
}

static void applyTokenEndpointSettings(
    httpcl::Config& conf,
    const httpcl::Config& requestConfForResource,
    const std::string& tokenOrRefreshUrl)
{
    // Base: persistent settings for the token/refresh URL (proxy/ssl/certs/cookies as configured
    // for issuer)
    conf = httpcl::Settings()[tokenOrRefreshUrl];

    // Enrich with safe pieces from the resource request (timeouts, proxy overrides, etc.)
    conf |= requestConfForResource;

    // Strip potentially conflicting Authorization (don’t leak resource headers to issuer)
    conf.headers.erase("Authorization");
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
        const auto b64 = httplib::detail::base64_encode(
            reinterpret_cast<const unsigned char*>(cred.data()),
            static_cast<unsigned>(cred.size()));
        conf.headers.insert({"Authorization", "Basic " + b64});
    }
}

OAuth2ClientCredentialsHandler::MintedToken OAuth2ClientCredentialsHandler::fetchToken(
    httpcl::IHttpClient& http,
    const httpcl::Config& requestConfForResource,
    const std::string& tokenUrl,
    const httpcl::Config::OAuth2& cc,
    const std::vector<std::string>& scopes)
{
    httpcl::Config conf;
    applyTokenEndpointSettings(conf, requestConfForResource, tokenUrl);

    std::string secret;
    addClientAuthIfSecretPresent(conf, cc, secret);

    std::string body = "grant_type=client_credentials";
    if (!scopes.empty())
        body += "&scope=" + urlEncode(joinScopes(scopes));
    if (!cc.audience.empty())
        body += "&audience=" + urlEncode(cc.audience);
    if (secret.empty()) {  // public client: send id
        body += "&client_id=" + urlEncode(cc.clientId);
    }

    auto res = http.post(
        tokenUrl,
        httpcl::IHttpClient::BodyAndContentType{body, "application/x-www-form-urlencoded"},
        conf);

    if (res.status < 200 || res.status >= 300) {
        throw httpcl::IHttpClient::Error(res, "OAuth2 token endpoint returned non-2xx.");
    }

    // Minimal JSON extraction (swap to your JSON lib when convenient)
    std::smatch m;
    MintedToken out;
    if (std::regex_search(res.content, m, std::regex(R"("access_token"\s*:\s*"([^"]+)")")))
        out.accessToken = m[1];
    if (out.accessToken.empty())
        throw std::runtime_error("OAuth2: access_token missing in response.");

    int expiresIn = 3600;
    if (std::regex_search(res.content, m, std::regex(R"("expires_in"\s*:\s*(\d+))")))
        expiresIn = std::stoi(m[1]);
    if (std::regex_search(res.content, m, std::regex(R"("refresh_token"\s*:\s*"([^"]+)")")))
        out.refreshToken = m[1];

    out.expiresAt = std::chrono::steady_clock::now() + seconds(expiresIn - 30);
    return out;
}

OAuth2ClientCredentialsHandler::MintedToken OAuth2ClientCredentialsHandler::refreshToken(
    httpcl::IHttpClient& http,
    const httpcl::Config& requestConfForResource,
    const std::string& refreshUrl,
    const httpcl::Config::OAuth2& cc,
    const std::string& refreshTok)
{
    httpcl::Config conf;
    applyTokenEndpointSettings(conf, requestConfForResource, refreshUrl);

    std::string secret;
    addClientAuthIfSecretPresent(conf, cc, secret);

    std::string body = "grant_type=refresh_token&refresh_token=" + urlEncode(refreshTok);
    if (secret.empty()) {  // public client
        body += "&client_id=" + urlEncode(cc.clientId);
    }

    auto res = http.post(
        refreshUrl,
        httpcl::IHttpClient::BodyAndContentType{body, "application/x-www-form-urlencoded"},
        conf);

    if (res.status < 200 || res.status >= 300) {
        throw httpcl::IHttpClient::Error(res, "OAuth2 refresh endpoint returned non-2xx.");
    }

    // Parse like fetchToken (some issuers reissue refresh_token, some don't)
    std::smatch m;
    MintedToken out;
    if (std::regex_search(res.content, m, std::regex(R"("access_token"\s*:\s*"([^"]+)")")))
        out.accessToken = m[1];
    if (out.accessToken.empty())
        throw std::runtime_error("OAuth2: access_token missing in refresh response.");

    int expiresIn = 3600;
    if (std::regex_search(res.content, m, std::regex(R"("expires_in"\s*:\s*(\d+))")))
        expiresIn = std::stoi(m[1]);
    out.expiresAt = std::chrono::steady_clock::now() + seconds(expiresIn - 30);

    // Prefer newly returned refresh_token if present, else keep the old one
    std::string newRefresh = refreshTok;
    if (std::regex_search(res.content, m, std::regex(R"("refresh_token"\s*:\s*"([^"]+)")")))
        newRefresh = m[1];
    out.refreshToken = std::move(newRefresh);
    return out;
}

}  // namespace zswagcl