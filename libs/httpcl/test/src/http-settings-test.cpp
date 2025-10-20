#include <catch2/catch_all.hpp>

#include "httpcl/http-settings.hpp"

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
