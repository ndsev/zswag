#include <catch2/catch_all.hpp>

#include "httpcl/http-settings.hpp"

#include <cstdlib>
#include <optional>
#include <string>

namespace {

void setTestEnv(std::string const& name, std::string const& value)
{
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

void unsetTestEnv(std::string const& name)
{
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

class ScopedEnv
{
public:
    ScopedEnv(std::string name, std::string value)
        : name_(std::move(name))
    {
        if (auto const* oldValue = std::getenv(name_.c_str()))
            oldValue_ = oldValue;
        setTestEnv(name_, value);
    }

    ~ScopedEnv()
    {
        if (oldValue_)
            setTestEnv(name_, *oldValue_);
        else
            unsetTestEnv(name_);
    }

private:
    std::string name_;
    std::optional<std::string> oldValue_;
};

}

TEST_CASE("HTTP settings expand $env references", "[http-settings][env]") {
    ScopedEnv user("ZSWAG_TEST_HTTP_USER", "alice");
    ScopedEnv password("ZSWAG_TEST_HTTP_PASSWORD", "secret");
    ScopedEnv host("ZSWAG_TEST_HTTP_HOST", "api.example.com");
    ScopedEnv port("ZSWAG_TEST_HTTP_PORT", "3128");
    ScopedEnv nonce("ZSWAG_TEST_HTTP_NONCE", "24");

    std::string yaml = R"(
scope: https://$env.ZSWAG_TEST_HTTP_HOST/*
basic-auth:
  user: $env.ZSWAG_TEST_HTTP_USER
  password: prefix-$env.ZSWAG_TEST_HTTP_PASSWORD
proxy:
  host: proxy.$env.ZSWAG_TEST_HTTP_HOST
  port: $env.ZSWAG_TEST_HTTP_PORT
  user: proxy-$env.ZSWAG_TEST_HTTP_USER
  password: $env.ZSWAG_TEST_HTTP_PASSWORD
headers:
  X-Token: Bearer $env.ZSWAG_TEST_HTTP_PASSWORD
query:
  token: $env.ZSWAG_TEST_HTTP_PASSWORD
cookies:
  session: $env.ZSWAG_TEST_HTTP_PASSWORD
api-key: key-$env.ZSWAG_TEST_HTTP_PASSWORD
oauth2:
  clientId: client-$env.ZSWAG_TEST_HTTP_USER
  clientSecret: $env.ZSWAG_TEST_HTTP_PASSWORD
  tokenUrl: https://issuer.$env.ZSWAG_TEST_HTTP_HOST/token
  audience: aud-$env.ZSWAG_TEST_HTTP_USER
  scope:
    - read-$env.ZSWAG_TEST_HTTP_USER
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: $env.ZSWAG_TEST_HTTP_NONCE
)";

    httpcl::Config cfg(yaml);

    REQUIRE(cfg.scope == "https://api.example.com/*");
    REQUIRE(cfg.auth.has_value());
    REQUIRE(cfg.auth->user == "alice");
    REQUIRE(cfg.auth->password == "prefix-secret");
    REQUIRE(cfg.proxy.has_value());
    REQUIRE(cfg.proxy->host == "proxy.api.example.com");
    REQUIRE(cfg.proxy->port == 3128);
    REQUIRE(cfg.proxy->user == "proxy-alice");
    REQUIRE(cfg.proxy->password == "secret");
    REQUIRE(cfg.headers.find("X-Token")->second == "Bearer secret");
    REQUIRE(cfg.query.find("token")->second == "secret");
    REQUIRE(cfg.cookies["session"] == "secret");
    REQUIRE(cfg.apiKey == "key-secret");
    REQUIRE(cfg.oauth2.has_value());
    REQUIRE(cfg.oauth2->clientId == "client-alice");
    REQUIRE(cfg.oauth2->clientSecret == "secret");
    REQUIRE(cfg.oauth2->tokenUrlOverride == "https://issuer.api.example.com/token");
    REQUIRE(cfg.oauth2->audience == "aud-alice");
    REQUIRE(cfg.oauth2->scopesOverride == std::vector<std::string>{"read-alice"});
    REQUIRE(cfg.oauth2->tokenEndpointAuth.has_value());
    REQUIRE(cfg.oauth2->tokenEndpointAuth->nonceLength == 24);
}

TEST_CASE("HTTP settings reject unresolved $env references", "[http-settings][env]") {
    unsetTestEnv("ZSWAG_TEST_HTTP_MISSING");

    REQUIRE_THROWS_WITH(
        httpcl::Config("api-key: $env.ZSWAG_TEST_HTTP_MISSING"),
        Catch::Matchers::ContainsSubstring("referenced by HTTP settings is not set")
    );
}

TEST_CASE("OAuth2 tokenEndpointAuth configuration", "[http-settings][oauth2]") {

    SECTION("Default to Rfc6749_ClientSecretBasic when tokenEndpointAuth omitted") {
        std::string yaml = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.oauth2.has_value());
        REQUIRE(cfg.oauth2->getTokenEndpointAuthMethod() ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc6749_ClientSecretBasic);
        REQUIRE_FALSE(cfg.oauth2->tokenEndpointAuth.has_value());
    }

    SECTION("Parse tokenEndpointAuth with method rfc6749-client-secret-basic") {
        std::string yaml = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc6749-client-secret-basic
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.oauth2.has_value());
        REQUIRE(cfg.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc6749_ClientSecretBasic);
        REQUIRE(cfg.oauth2->tokenEndpointAuth->nonceLength == 16);  // Default
    }

    SECTION("Parse tokenEndpointAuth with method rfc5849-oauth1-signature") {
        std::string yaml = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.oauth2.has_value());
        REQUIRE(cfg.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
        REQUIRE(cfg.oauth2->tokenEndpointAuth->nonceLength == 16);  // Default
    }

    SECTION("Parse tokenEndpointAuth with custom nonceLength") {
        std::string yaml = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 32
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.oauth2.has_value());
        REQUIRE(cfg.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
        REQUIRE(cfg.oauth2->tokenEndpointAuth->nonceLength == 32);
    }

    SECTION("Reject invalid tokenEndpointAuth method") {
        std::string yaml = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: invalid-method
)";

        REQUIRE_THROWS_WITH(
            httpcl::Config(yaml),
            Catch::Matchers::ContainsSubstring("Unknown tokenEndpointAuth method")
        );
    }

    SECTION("Reject nonceLength too small") {
        std::string yaml = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 7
)";

        REQUIRE_THROWS_WITH(
            httpcl::Config(yaml),
            Catch::Matchers::ContainsSubstring("nonceLength must be between 8 and 64")
        );
    }

    SECTION("Reject nonceLength too large") {
        std::string yaml = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 65
)";

        REQUIRE_THROWS_WITH(
            httpcl::Config(yaml),
            Catch::Matchers::ContainsSubstring("nonceLength must be between 8 and 64")
        );
    }

    SECTION("Accept nonceLength at boundary values") {
        // Test minimum
        std::string yaml_min = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 8
)";
        httpcl::Config cfg_min(yaml_min);
        REQUIRE(cfg_min.oauth2->tokenEndpointAuth->nonceLength == 8);

        // Test maximum
        std::string yaml_max = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 64
)";
        httpcl::Config cfg_max(yaml_max);
        REQUIRE(cfg_max.oauth2->tokenEndpointAuth->nonceLength == 64);
    }

    SECTION("OAuth2 with OAuth1 signature example configuration") {
        std::string yaml = R"(
scope: https://api.example.com/*
oauth2:
  clientId: test-access-key-id
  clientSecret: test-access-key-secret
  tokenUrl: https://auth.example.com/oauth2/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.oauth2.has_value());
        REQUIRE(cfg.oauth2->clientId == "test-access-key-id");
        REQUIRE(cfg.oauth2->tokenUrlOverride == "https://auth.example.com/oauth2/token");
        REQUIRE(cfg.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
    }
}

TEST_CASE("OAuth2 tokenEndpointAuth YAML serialization", "[http-settings][oauth2]") {

    SECTION("toYaml() with tokenEndpointAuth rfc5849-oauth1-signature") {
        std::string yaml_in = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.oauth2.has_value());
        REQUIRE(cfg2.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg2.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
    }

    SECTION("toYaml() with tokenEndpointAuth rfc6749-client-secret-basic") {
        std::string yaml_in = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc6749-client-secret-basic
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.oauth2.has_value());
        REQUIRE(cfg2.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg2.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc6749_ClientSecretBasic);
    }

    SECTION("toYaml() with custom nonceLength") {
        std::string yaml_in = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 32
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.oauth2.has_value());
        REQUIRE(cfg2.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg2.oauth2->tokenEndpointAuth->nonceLength == 32);
    }

    SECTION("toYaml() omits default nonceLength") {
        std::string yaml_in = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 16
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Default nonceLength should not appear in YAML output
        REQUIRE(yaml_out.find("nonceLength") == std::string::npos);

        // But parse it back and verify default is preserved
        httpcl::Config cfg2(yaml_out);
        REQUIRE(cfg2.oauth2->tokenEndpointAuth->nonceLength == 16);
    }

    SECTION("toYaml() without tokenEndpointAuth") {
        std::string yaml_in = R"(
oauth2:
  clientId: test-client
  clientSecret: test-secret
  tokenUrl: https://example.com/token
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // tokenEndpointAuth should not appear in YAML output
        REQUIRE(yaml_out.find("tokenEndpointAuth") == std::string::npos);

        // Parse it back and verify defaults
        httpcl::Config cfg2(yaml_out);
        REQUIRE(cfg2.oauth2.has_value());
        REQUIRE_FALSE(cfg2.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(cfg2.oauth2->getTokenEndpointAuthMethod() ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc6749_ClientSecretBasic);
    }
}

TEST_CASE("OAuth2 tokenEndpointAuth config merge", "[http-settings][oauth2]") {

    SECTION("Merge preserves tokenEndpointAuth") {
        std::string yaml_base = R"(
oauth2:
  clientId: base-client
  clientSecret: base-secret
  tokenUrl: https://base.example.com/token
)";
        std::string yaml_override = R"(
oauth2:
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 24
)";

        httpcl::Config base(yaml_base);
        httpcl::Config override(yaml_override);

        base |= override;

        REQUIRE(base.oauth2.has_value());
        REQUIRE(base.oauth2->clientId == "base-client");
        REQUIRE(base.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(base.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
        REQUIRE(base.oauth2->tokenEndpointAuth->nonceLength == 24);
    }

    SECTION("Merge replaces tokenEndpointAuth") {
        std::string yaml_base = R"(
oauth2:
  clientId: base-client
  tokenEndpointAuth:
    method: rfc6749-client-secret-basic
)";
        std::string yaml_override = R"(
oauth2:
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
    nonceLength: 20
)";

        httpcl::Config base(yaml_base);
        httpcl::Config override(yaml_override);

        base |= override;

        REQUIRE(base.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(base.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
        REQUIRE(base.oauth2->tokenEndpointAuth->nonceLength == 20);
    }

    SECTION("Merge without tokenEndpointAuth in override keeps original") {
        std::string yaml_base = R"(
oauth2:
  clientId: base-client
  tokenEndpointAuth:
    method: rfc5849-oauth1-signature
)";
        std::string yaml_override = R"(
oauth2:
  clientSecret: override-secret
)";

        httpcl::Config base(yaml_base);
        httpcl::Config override(yaml_override);

        base |= override;

        REQUIRE(base.oauth2->tokenEndpointAuth.has_value());
        REQUIRE(base.oauth2->tokenEndpointAuth->method ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
        REQUIRE(base.oauth2->clientSecret == "override-secret");
    }
}

TEST_CASE("OAuth2 getTokenEndpointAuthMethod helper", "[http-settings][oauth2]") {

    SECTION("Returns Rfc6749_ClientSecretBasic when tokenEndpointAuth not set") {
        httpcl::Config::OAuth2 oauth2;
        oauth2.clientId = "test";

        REQUIRE(oauth2.getTokenEndpointAuthMethod() ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc6749_ClientSecretBasic);
    }

    SECTION("Returns configured method when tokenEndpointAuth set") {
        httpcl::Config::OAuth2 oauth2;
        oauth2.clientId = "test";
        oauth2.tokenEndpointAuth = httpcl::Config::OAuth2::TokenEndpointAuth{
            httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature,
            16
        };

        REQUIRE(oauth2.getTokenEndpointAuthMethod() ==
                httpcl::Config::OAuth2::TokenEndpointAuthMethod::Rfc5849_Oauth1Signature);
    }
}

// ============================================================================
// Proxy Configuration Tests - Coverage for http-settings.cpp:69-84
// ============================================================================

TEST_CASE("Proxy configuration encode/decode", "[http-settings][proxy]") {

    SECTION("Proxy with host and port only") {
        std::string yaml = R"(
proxy:
  host: proxy.example.com
  port: 8080
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.proxy.has_value());
        REQUIRE(cfg.proxy->host == "proxy.example.com");
        REQUIRE(cfg.proxy->port == 8080);
        REQUIRE(cfg.proxy->user.empty());
        REQUIRE(cfg.proxy->password.empty());
        REQUIRE(cfg.proxy->keychain.empty());
    }

    SECTION("Proxy with user and password") {
        std::string yaml = R"(
proxy:
  host: proxy.example.com
  port: 3128
  user: proxyuser
  password: proxypass
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.proxy.has_value());
        REQUIRE(cfg.proxy->host == "proxy.example.com");
        REQUIRE(cfg.proxy->port == 3128);
        REQUIRE(cfg.proxy->user == "proxyuser");
        REQUIRE(cfg.proxy->password == "proxypass");
        REQUIRE(cfg.proxy->keychain.empty());
    }

    SECTION("Proxy with user and keychain") {
        std::string yaml = R"(
proxy:
  host: secure-proxy.example.com
  port: 8443
  user: keychainuser
  keychain: my-proxy-keychain-entry
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.proxy.has_value());
        REQUIRE(cfg.proxy->host == "secure-proxy.example.com");
        REQUIRE(cfg.proxy->port == 8443);
        REQUIRE(cfg.proxy->user == "keychainuser");
        REQUIRE(cfg.proxy->password.empty());
        REQUIRE(cfg.proxy->keychain == "my-proxy-keychain-entry");
    }

    SECTION("Proxy toYaml roundtrip with host/port only") {
        std::string yaml_in = R"(
proxy:
  host: proxy.example.com
  port: 8080
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.proxy.has_value());
        REQUIRE(cfg2.proxy->host == "proxy.example.com");
        REQUIRE(cfg2.proxy->port == 8080);
    }

    SECTION("Proxy toYaml roundtrip with user/password") {
        std::string yaml_in = R"(
proxy:
  host: proxy.example.com
  port: 3128
  user: myuser
  password: mypassword
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.proxy.has_value());
        REQUIRE(cfg2.proxy->host == "proxy.example.com");
        REQUIRE(cfg2.proxy->port == 3128);
        REQUIRE(cfg2.proxy->user == "myuser");
        REQUIRE(cfg2.proxy->password == "mypassword");
    }

    SECTION("Proxy toYaml roundtrip with user/keychain") {
        std::string yaml_in = R"(
proxy:
  host: proxy.example.com
  port: 8443
  user: keychainuser
  keychain: my-keychain-entry
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.proxy.has_value());
        REQUIRE(cfg2.proxy->host == "proxy.example.com");
        REQUIRE(cfg2.proxy->port == 8443);
        REQUIRE(cfg2.proxy->user == "keychainuser");
        REQUIRE(cfg2.proxy->keychain == "my-keychain-entry");
    }

    SECTION("Proxy missing host fails") {
        std::string yaml = R"(
proxy:
  port: 8080
)";
        // YAML-CPP throws on missing required fields
        REQUIRE_THROWS(httpcl::Config(yaml));
    }

    SECTION("Proxy missing port fails") {
        std::string yaml = R"(
proxy:
  host: proxy.example.com
)";
        // YAML-CPP throws on missing required fields
        REQUIRE_THROWS(httpcl::Config(yaml));
    }
}

// ============================================================================
// BasicAuthentication Configuration Tests - Coverage for http-settings.cpp
// ============================================================================

TEST_CASE("BasicAuthentication configuration encode/decode", "[http-settings][auth]") {

    SECTION("BasicAuthentication with user and password") {
        std::string yaml = R"(
basic-auth:
  user: testuser
  password: testpass
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.auth.has_value());
        REQUIRE(cfg.auth->user == "testuser");
        REQUIRE(cfg.auth->password == "testpass");
        REQUIRE(cfg.auth->keychain.empty());
    }

    SECTION("BasicAuthentication with user and keychain") {
        std::string yaml = R"(
basic-auth:
  user: keychainuser
  keychain: my-keychain-entry
)";
        httpcl::Config cfg(yaml);

        REQUIRE(cfg.auth.has_value());
        REQUIRE(cfg.auth->user == "keychainuser");
        REQUIRE(cfg.auth->password.empty());
        REQUIRE(cfg.auth->keychain == "my-keychain-entry");
    }

    SECTION("BasicAuthentication toYaml roundtrip with password") {
        std::string yaml_in = R"(
basic-auth:
  user: testuser
  password: testpass
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.auth.has_value());
        REQUIRE(cfg2.auth->user == "testuser");
        REQUIRE(cfg2.auth->password == "testpass");
    }

    SECTION("BasicAuthentication toYaml roundtrip with keychain") {
        std::string yaml_in = R"(
basic-auth:
  user: keychainuser
  keychain: my-keychain-entry
)";
        httpcl::Config cfg(yaml_in);
        std::string yaml_out = cfg.toYaml();

        // Parse it back
        httpcl::Config cfg2(yaml_out);

        REQUIRE(cfg2.auth.has_value());
        REQUIRE(cfg2.auth->user == "keychainuser");
        REQUIRE(cfg2.auth->keychain == "my-keychain-entry");
    }

    SECTION("BasicAuthentication missing user fails") {
        std::string yaml = R"(
basic-auth:
  password: testpass
)";
        // YAML-CPP throws on missing required fields
        REQUIRE_THROWS(httpcl::Config(yaml));
    }

    SECTION("BasicAuthentication missing password and keychain fails") {
        std::string yaml = R"(
basic-auth:
  user: testuser
)";
        // YAML-CPP throws on missing required fields
        REQUIRE_THROWS(httpcl::Config(yaml));
    }
}
