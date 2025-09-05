#include <catch2/catch_all.hpp>

#include <fstream>
#include <filesystem>
#include <thread>

#include "zswagcl/oaclient.hpp"
#include "zswagcl/private/openapi-parser.hpp"
#include "httpcl/http-settings.hpp"
#include "service_client_test/Request.h"

using namespace zswagcl;
namespace fs = std::filesystem;

/**
 * Integration tests for OAuth2 with real OpenAPI specs and HTTP settings
 */
TEST_CASE("OAuth2 Integration with OpenAPI Parser", "[oauth2][integration]") {
    
    SECTION("Parse OAuth2 Security Scheme from OpenAPI Spec") {
        std::ifstream file(TESTDATA "/oauth2-openapi.yaml");
        REQUIRE(file.is_open());
        
        OpenAPIConfig config;
        REQUIRE_NOTHROW(config = parseOpenAPIConfig(file));
        
        // Verify OAuth2 security scheme was parsed correctly
        REQUIRE(config.securitySchemes.size() == 1);
        REQUIRE(config.securitySchemes.count("oauth2") > 0);
        
        auto scheme = config.securitySchemes["oauth2"];
        REQUIRE(scheme->type == OpenAPIConfig::SecuritySchemeType::OAuth2ClientCredentials);
        REQUIRE(scheme->oauthTokenUrl == "https://auth.example.com/token");
        REQUIRE(scheme->oauthRefreshUrl == "https://auth.example.com/refresh");
        REQUIRE(scheme->oauthScopes.size() == 3);
        REQUIRE(scheme->oauthScopes.count("read") > 0);
        REQUIRE(scheme->oauthScopes.count("write") > 0);
        REQUIRE(scheme->oauthScopes.count("admin") > 0);
        
        // Verify endpoint security requirements
        REQUIRE(config.methodPath.count("protectedEndpoint") > 0);
        auto& protectedPath = config.methodPath["protectedEndpoint"];
        REQUIRE(protectedPath.security.has_value());
        REQUIRE(protectedPath.security->size() == 1);
        REQUIRE(protectedPath.security->at(0).size() == 1);
        REQUIRE(protectedPath.security->at(0)[0].scheme == scheme);
        REQUIRE(protectedPath.security->at(0)[0].scopes.size() == 2);
        
        // Public endpoint should have no security
        REQUIRE(config.methodPath.count("publicEndpoint") > 0);
        auto& publicPath = config.methodPath["publicEndpoint"];
        REQUIRE_FALSE(publicPath.security.has_value());
    }
    
    SECTION("Load OAuth2 Settings from YAML") {
        // Set environment variable for settings file
        std::string settingsPath = std::string(TESTDATA) + "/oauth2-settings.yaml";
        
#if _MSC_VER
        std::string envVar = "HTTP_SETTINGS_FILE=" + settingsPath;
        _putenv(envVar.c_str());
#else
        setenv("HTTP_SETTINGS_FILE", settingsPath.c_str(), 1);
#endif
        
        httpcl::Settings settings;
        REQUIRE_NOTHROW(settings.load());
        
        // Test OAuth2 config for auth.example.com
        auto config = settings["https://auth.example.com/token"];
        REQUIRE(config.oauth2.has_value());
        REQUIRE(config.oauth2->clientId == "test-client");
        REQUIRE(config.oauth2->clientSecret == "test-secret");
        REQUIRE(config.oauth2->tokenUrlOverride == "https://auth.example.com/token");
        REQUIRE(config.oauth2->refreshUrlOverride == "https://auth.example.com/refresh");
        REQUIRE(config.oauth2->audience == "https://api.example.com");
        REQUIRE(config.oauth2->scopesOverride.size() == 2);
        
        // Test OAuth2 config with keychain
        auto secureConfig = settings["https://secure-auth.example.com/token"];
        REQUIRE(secureConfig.oauth2.has_value());
        REQUIRE(secureConfig.oauth2->clientId == "secure-client");
        REQUIRE(secureConfig.oauth2->clientSecret.empty());
        REQUIRE(secureConfig.oauth2->clientSecretKeychain == "test-oauth-secret");
        
        // Test public client config
        auto publicConfig = settings["https://public-auth.example.com/token"];
        REQUIRE(publicConfig.oauth2.has_value());
        REQUIRE(publicConfig.oauth2->clientId == "public-client");
        REQUIRE(publicConfig.oauth2->clientSecret.empty());
        REQUIRE(publicConfig.oauth2->clientSecretKeychain.empty());
    }
    
    SECTION("End-to-End OAuth2 Flow with Mock Server") {
        // Load OpenAPI spec
        std::ifstream specFile(TESTDATA "/oauth2-openapi.yaml");
        auto config = parseOpenAPIConfig(specFile);
        
        // Setup HTTP config with OAuth2
        httpcl::Config httpConfig;
        httpConfig.oauth2.emplace();
        httpConfig.oauth2->clientId = "integration-test-client";
        httpConfig.oauth2->clientSecret = "integration-test-secret";
        httpConfig.oauth2->audience = "https://api.example.com";
        httpConfig.oauth2->scopesOverride = {"read", "write", "admin"};

        // Track the flow
        bool tokenRequested = false;
        bool apiCalled = false;
        std::string issuedToken = "integration_test_token_12345";
        
        // Setup mock HTTP client
        auto client = std::make_unique<httpcl::MockHttpClient>();
        client->postFun = [&](const std::string_view& uri,
                             const httpcl::OptionalBodyAndContentType& body,
                             const httpcl::Config& conf) -> httpcl::IHttpClient::Result {
            
            // Token endpoint
            if (uri == "https://auth.example.com/token") {
                tokenRequested = true;
                
                // Verify client credentials
                REQUIRE(body.has_value());
                REQUIRE(body->contentType == "application/x-www-form-urlencoded");
                REQUIRE(body->body.find("grant_type=client_credentials") != std::string::npos);
                
                // Check for Basic auth header or client_id in body
                bool hasAuth = false;
                auto authHeader = conf.headers.find("Authorization");
                if (authHeader != conf.headers.end() && 
                    authHeader->second.find("Basic ") == 0) {
                    hasAuth = true;
                } else if (body->body.find("client_id=integration-test-client") != std::string::npos) {
                    hasAuth = true;
                }
                REQUIRE(hasAuth);
                
                // Return token response
                return {200, R"({
                    "access_token": ")" + issuedToken + R"(",
                    "token_type": "Bearer",
                    "expires_in": 3600,
                    "scope": "read write admin"
                })"};
            }
            
            // API endpoint
            if (uri.find("https://api.example.com") == 0) {
                apiCalled = true;
                
                // Verify Bearer token is present depending on public or protected endpoint
                auto authHeader = conf.headers.find("Authorization");
                if (uri.find("/public") != std::string::npos) {
                    REQUIRE(authHeader == conf.headers.end());
                }
                else {
                    REQUIRE(authHeader != conf.headers.end());
                    REQUIRE(authHeader->second == "Bearer " + issuedToken);
                }

                return {200, ""};
            }
            
            return {404, "Not found"};
        };
        
        // Create OAClient and make a protected call
        auto oaClient = OAClient(config, std::move(client), httpConfig);
        auto request = service_client_test::Request("test", 2, {"a", "b"},
                                                   service_client_test::Flat("user", "data"));
        
        // Call protected endpoint
        REQUIRE_NOTHROW(
            oaClient.callMethod("protectedEndpoint", 
                              zserio::ReflectableServiceData(request.reflectable()), 
                              nullptr)
        );
        
        REQUIRE(tokenRequested);
        REQUIRE(apiCalled);
        
        // Call public endpoint - should not request token
        tokenRequested = false;
        apiCalled = false;
        
        REQUIRE_NOTHROW(
            oaClient.callMethod("publicEndpoint", 
                              zserio::ReflectableServiceData(request.reflectable()), 
                              nullptr)
        );
        
        REQUIRE_FALSE(tokenRequested); // No token needed for public endpoint
        REQUIRE(apiCalled);
    }
    
    SECTION("Multiple Security Alternatives") {
        // Create config with multiple auth options (OAuth2 OR API Key)
        OpenAPIConfig config;
        config.servers.push_back(httpcl::URIComponents::fromStrRfc3986("https://api.example.com"));
        
        // OAuth2 scheme
        auto oauth2Scheme = std::make_shared<OpenAPIConfig::SecurityScheme>();
        oauth2Scheme->type = OpenAPIConfig::SecuritySchemeType::OAuth2ClientCredentials;
        oauth2Scheme->oauthTokenUrl = "https://auth.example.com/token";
        oauth2Scheme->id = "oauth2";
        config.securitySchemes["oauth2"] = oauth2Scheme;
        
        // API Key scheme
        auto apiKeyScheme = std::make_shared<OpenAPIConfig::SecurityScheme>();
        apiKeyScheme->type = OpenAPIConfig::SecuritySchemeType::ApiKeyHeader;
        apiKeyScheme->apiKeyName = "X-API-Key";
        apiKeyScheme->id = "apiKey";
        config.securitySchemes["apiKey"] = apiKeyScheme;
        
        // Create method with OR security (OAuth2 OR API Key)
        OpenAPIConfig::Path path;
        path.path = "/flexible";
        path.httpMethod = "POST";
        path.bodyRequestObject = true;
        
        OpenAPIConfig::SecurityAlternatives alts;
        
        // Alternative 1: OAuth2
        OpenAPIConfig::SecurityAlternative alt1;
        alt1.push_back({oauth2Scheme, {}});
        alts.push_back(alt1);
        
        // Alternative 2: API Key
        OpenAPIConfig::SecurityAlternative alt2;
        alt2.push_back({apiKeyScheme, {}});
        alts.push_back(alt2);
        
        path.security = alts;
        config.methodPath["flexible"] = path;
        
        // Test with API Key (no OAuth2 config)
        httpcl::Config httpConfigWithApiKey;
        httpConfigWithApiKey.headers.insert({"X-API-Key", "test-api-key"});
        
        auto client1 = std::make_unique<httpcl::MockHttpClient>();
        client1->postFun = [](auto, auto, const httpcl::Config& conf) {
            // Should have API key, not OAuth2
            REQUIRE(conf.headers.count("X-API-Key") > 0);
            REQUIRE(conf.headers.count("Authorization") == 0);
            return httpcl::IHttpClient::Result{200, ""};
        };
        
        auto oaClient1 = OAClient(config, std::move(client1), httpConfigWithApiKey);
        auto request = service_client_test::Request("test", 0, {}, 
                                                   service_client_test::Flat("", ""));
        
        REQUIRE_NOTHROW(
            oaClient1.callMethod("flexible", 
                               zserio::ReflectableServiceData(request.reflectable()), 
                               nullptr)
        );
        
        // Test with OAuth2 (no API key)
        httpcl::Config httpConfigWithOAuth;
        httpConfigWithOAuth.oauth2 = httpcl::Config::OAuth2{
            "client",
            "secret"
        };
        
        auto client2 = std::make_unique<httpcl::MockHttpClient>();
        bool tokenEndpointCalled = false;
        client2->postFun = [&](const std::string_view& uri, auto body, const httpcl::Config& conf) {
            if (uri == "https://auth.example.com/token") {
                tokenEndpointCalled = true;
                return httpcl::IHttpClient::Result{200, R"({
                    "access_token": "token123",
                    "expires_in": 3600
                })"};
            }
            // API call should have Bearer token, not API key
            REQUIRE(conf.headers.count("Authorization") > 0);
            REQUIRE(conf.headers.find("Authorization")->second == "Bearer token123");
            REQUIRE(conf.headers.count("X-API-Key") == 0);
            return httpcl::IHttpClient::Result{200, ""};
        };
        
        auto oaClient2 = OAClient(config, std::move(client2), httpConfigWithOAuth);
        
        REQUIRE_NOTHROW(
            oaClient2.callMethod("flexible", 
                               zserio::ReflectableServiceData(request.reflectable()), 
                               nullptr)
        );
        REQUIRE(tokenEndpointCalled);
    }
}
