#include "private/openapi-security.hpp"

#include <stx/format.h>

namespace zswagcl
{

namespace
{

using SecurityRequirement = OpenAPIConfig::SecurityRequirement;
using SecuritySchemeType = OpenAPIConfig::SecuritySchemeType;

class HttpBasicHandler final : public ISecurityHandler {
public:
    bool satisfy(const SecurityRequirement& req, AuthContext& ctx, std::string& mismatchReason) override {
        if (ctx.httpConfig.auth.has_value())
            return true;

        std::regex basicAuthValueRe{
            "^Basic .+$",
            std::regex_constants::ECMAScript|std::regex_constants::icase
        };

        auto found = std::any_of(ctx.httpConfig.headers.begin(), ctx.httpConfig.headers.end(), [&](auto const& headerNameAndValue){
            return headerNameAndValue.first == "Authorization" &&
                   std::regex_match(headerNameAndValue.second, basicAuthValueRe);
        });

        if (found)
            return true;

        mismatchReason = "HTTP basic-auth credentials are missing.";
        return false;
    }
};

class HttpBearerHandler final : public ISecurityHandler {
public:
    bool satisfy(const SecurityRequirement& req, AuthContext& ctx, std::string& mismatchReason) override {
        std::regex bearerValueRe{
            "^Bearer .+$",
            std::regex_constants::ECMAScript|std::regex_constants::icase
        };

        auto found = std::any_of(ctx.httpConfig.headers.begin(), ctx.httpConfig.headers.end(), [&](auto const& headerNameAndValue){
            return headerNameAndValue.first == "Authorization" &&
                   std::regex_match(headerNameAndValue.second, bearerValueRe);
        });

        if (found)
            return true;
        mismatchReason = "Header `Authorization: Bearer ...` is missing.";
        return false;
    }
};

class ApiKeyHandler final : public ISecurityHandler {
public:
    bool satisfy(const SecurityRequirement& req, AuthContext& ctx, std::string& mismatchReason) override {
        const auto& s = *req.scheme;

        // Convenience: support the global apiKey shortcut if your Config has one (ctx.httpConfig.apiKey)
        auto ensure = [&](auto& container, const std::string& keyName, char const* containerName) -> bool {
            if (container.find(keyName) != container.end()) {
                return true;
            }
            if (ctx.httpConfig.apiKey) {
                container.insert({keyName, *ctx.httpConfig.apiKey});
                return true;
            }
            mismatchReason = stx::format("API key ({}) missing: {}", containerName, s.apiKeyName);
            return false;
        };

        switch (s.type) {
            case SecuritySchemeType::ApiKeyQuery:
                return ensure(ctx.httpConfig.query, s.apiKeyName, "query");
            case SecuritySchemeType::ApiKeyHeader:
                return ensure(ctx.httpConfig.headers, s.apiKeyName, "headers");
            case SecuritySchemeType::ApiKeyCookie:
                return ensure(ctx.httpConfig.cookies, s.apiKeyName, "cookies");
            default:
                mismatchReason = "Unsupported apiKey parameter location.";
                return false;
        }
    }
};

}

AuthRegistry::AuthRegistry()
{
    handlers_.insert({SecuritySchemeType::HttpBasic, std::make_unique<HttpBasicHandler>()});
    handlers_.insert({SecuritySchemeType::HttpBearer, std::make_unique<HttpBearerHandler>()});
    handlers_.insert({SecuritySchemeType::ApiKeyQuery, std::make_unique<ApiKeyHandler>()});
    handlers_.insert({SecuritySchemeType::ApiKeyHeader, std::make_unique<ApiKeyHandler>()});
    handlers_.insert({SecuritySchemeType::ApiKeyCookie, std::make_unique<ApiKeyHandler>()});
}

void AuthRegistry::satisfySecurity(
    const OpenAPIConfig::SecurityAlternatives& alts, AuthContext const& ctx)
{
    if (alts.empty())
        return; // Nothing to check

    bool anyAlternativeMatched = false;
    std::stringstream error;
    error << "The provided HTTP configuration does not satisfy authentication requirements:\n";

    int i = 0;
    for (auto const& schemeSet : alts)
    {
        bool matched = true;

        for (auto const& req : schemeSet) {
            std::string reasonForMismatch;
            auto handlerIt = handlers_.find(req.scheme->type);
            if (handlerIt == handlers_.end()) {
                reasonForMismatch = stx::format(
                    "No handler registered for required security scheme {}",
                    req.scheme->id);
                continue;
            }

            // Note: Handlers may mutate ctx.httpConf
            if (!handlerIt->second->satisfy(req, ctx, reasonForMismatch)) {
                error << "  In security configuration " << i << ": " << reasonForMismatch << "\n";
                matched = false;
                break;
            }
        }

        if (matched) {
            anyAlternativeMatched = true;
            break;
        }
        ++i;
    }

    if (!anyAlternativeMatched)
        throw std::runtime_error(error.str());
}

}  // namespace zswagcl