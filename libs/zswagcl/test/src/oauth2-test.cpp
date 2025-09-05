#include <catch2/catch_all.hpp>

#include <fstream>
#include <chrono>
#include <thread>
#include <sstream>

#include "zswagcl/oaclient.hpp"
#include "zswagcl/private/openapi-oauth.hpp"
#include "httpcl/http-client.hpp"
#include "yaml-cpp/yaml.h"
#include "service_client_test/Request.h"

using namespace zswagcl;
using namespace std::chrono_literals;

/**
 * Mock OAuth2 Authorization Server
 * 
 * This simulates an OAuth2 server that responds to token requests
 * and refresh requests according to the OAuth2 client credentials flow.
 */
class MockOAuth2Server {
public:
    struct TokenResponse {
        std::string access_token;
        std::string refresh_token;
        int expires_in;
        std::string token_type = "Bearer";
        std::string scope;
    };

    // Configuration for the mock server
    bool shouldFailTokenRequest = false;
    bool shouldFailRefreshRequest = false;
    bool shouldReturnInvalidResponse = false;
    bool shouldReturnRefreshToken = true;
    int tokenExpirySeconds = 3600;
    std::string expectedClientId = "test-client";
    std::string expectedClientSecret = "test-secret";
    std::string expectedAudience;
    std::vector<std::string> expectedScopes;

    // Tracking for test assertions
    int tokenRequestCount = 0;
    int refreshRequestCount = 0;
    std::string lastTokenRequestBody;
    std::string lastRefreshRequestBody;
    httpcl::Config lastTokenRequestConfig;
    httpcl::Config lastRefreshRequestConfig;

    // Generated tokens for verification
    std::string lastIssuedAccessToken;
    std::string lastIssuedRefreshToken;

    httpcl::IHttpClient::Result handleTokenRequest(
        const std::string_view& uri,
        const httpcl::OptionalBodyAndContentType& body,
        const httpcl::Config& config)
    {
        tokenRequestCount++;
        lastTokenRequestBody = body ? body->body : "";
        lastTokenRequestConfig = config;

        if (shouldFailTokenRequest) {
            return {400, R"({"error":"invalid_client","error_description":"Invalid client credentials"})"};
        }

        if (shouldReturnInvalidResponse) {
            return {200, R"({"invalid_field":"no_access_token_here"})"};
        }

        // Parse the request body
        std::map<std::string, std::string> params;
        parseFormUrlEncoded(lastTokenRequestBody, params);

        // Validate grant type
        if (params["grant_type"] != "client_credentials") {
            return {400, R"({"error":"unsupported_grant_type"})"};
        }

        // Check client authentication (either Basic auth or client_id in body)
        bool authenticated = false;
        if (config.auth && config.auth->user == expectedClientId && 
            config.auth->password == expectedClientSecret) {
            authenticated = true;
        } else {
            // Check for Basic auth header
            auto authHeader = config.headers.find("Authorization");
            if (authHeader != config.headers.end()) {
                // Parse Basic auth header
                if (authHeader->second.find("Basic ") == 0) {
                    // In real implementation, would decode base64 and check credentials
                    authenticated = true;
                }
            } else if (params["client_id"] == expectedClientId) {
                // Public client flow
                authenticated = true;
            }
        }

        if (!authenticated) {
            return {401, R"({"error":"invalid_client"})"};
        }

        // Check audience if expected
        if (!expectedAudience.empty() && params["audience"] != expectedAudience) {
            return {400, R"({"error":"invalid_audience"})"};
        }

        // Generate token response
        TokenResponse response;
        response.access_token = "access_" + std::to_string(tokenRequestCount);
        response.refresh_token = shouldReturnRefreshToken ? "refresh_" + std::to_string(tokenRequestCount) : "";
        response.expires_in = tokenExpirySeconds;
        response.scope = params["scope"];

        lastIssuedAccessToken = response.access_token;
        lastIssuedRefreshToken = response.refresh_token;

        return {200, serializeTokenResponse(response)};
    }

    httpcl::IHttpClient::Result handleRefreshRequest(
        const std::string_view& uri,
        const httpcl::OptionalBodyAndContentType& body,
        const httpcl::Config& config)
    {
        refreshRequestCount++;
        lastRefreshRequestBody = body ? body->body : "";
        lastRefreshRequestConfig = config;

        if (shouldFailRefreshRequest) {
            return {400, R"({"error":"invalid_grant","error_description":"Refresh token expired"})"};
        }

        // Parse the request body
        std::map<std::string, std::string> params;
        parseFormUrlEncoded(lastRefreshRequestBody, params);

        // Validate grant type
        if (params["grant_type"] != "refresh_token") {
            return {400, R"({"error":"unsupported_grant_type"})"};
        }

        // Check refresh token
        if (params["refresh_token"].empty()) {
            return {400, R"({"error":"invalid_request","error_description":"Missing refresh_token"})"};
        }

        // Generate new token
        TokenResponse response;
        response.access_token = "refreshed_access_" + std::to_string(refreshRequestCount);
        response.refresh_token = shouldReturnRefreshToken ? 
            "refreshed_refresh_" + std::to_string(refreshRequestCount) : "";
        response.expires_in = tokenExpirySeconds;

        lastIssuedAccessToken = response.access_token;
        lastIssuedRefreshToken = response.refresh_token;

        return {200, serializeTokenResponse(response)};
    }

private:
    void parseFormUrlEncoded(const std::string& body, std::map<std::string, std::string>& params) {
        std::istringstream stream(body);
        std::string pair;
        while (std::getline(stream, pair, '&')) {
            auto pos = pair.find('=');
            if (pos != std::string::npos) {
                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);
                // Simple URL decode (not complete, but sufficient for tests)
                std::replace(value.begin(), value.end(), '+', ' ');
                params[key] = value;
            }
        }
    }

    std::string serializeTokenResponse(const TokenResponse& response) {
        YAML::Node node;
        node["access_token"] = response.access_token;
        node["token_type"] = response.token_type;
        node["expires_in"] = response.expires_in;
        if (!response.refresh_token.empty()) {
            node["refresh_token"] = response.refresh_token;
        }
        if (!response.scope.empty()) {
            node["scope"] = response.scope;
        }
        
        YAML::Emitter out;
        out << YAML::Flow << node;
        return out.c_str();
    }
};

/**
 * Helper to create OpenAPI config with OAuth2
 */
static OpenAPIConfig makeOAuth2Config(
    const std::string& tokenUrl,
    const std::string& refreshUrl = "",
    const std::vector<std::string>& scopes = {})
{
    OpenAPIConfig config;
    config.servers.push_back(httpcl::URIComponents::fromStrRfc3986("https://api.example.com"));
    
    // Create OAuth2 security scheme
    auto scheme = std::make_shared<OpenAPIConfig::SecurityScheme>();
    scheme->type = OpenAPIConfig::SecuritySchemeType::OAuth2ClientCredentials;
    scheme->oauthTokenUrl = tokenUrl;
    scheme->oauthRefreshUrl = refreshUrl.empty() ? tokenUrl : refreshUrl;
    scheme->id = "oauth2";
    
    // Add scopes
    for (const auto& scope : scopes) {
        scheme->oauthScopes[scope] = "Test scope " + scope;
    }
    
    config.securitySchemes["oauth2"] = scheme;
    
    // Create a test method with OAuth2 requirement
    OpenAPIConfig::Path path;
    path.path = "/test";
    path.httpMethod = "POST";
    path.bodyRequestObject = true;
    
    OpenAPIConfig::SecurityRequirement req;
    req.scheme = scheme;
    req.scopes = scopes;
    
    OpenAPIConfig::SecurityAlternative alt;
    alt.push_back(req);
    
    OpenAPIConfig::SecurityAlternatives alts;
    alts.push_back(alt);
    
    path.security = alts;
    config.methodPath["test"] = path;
    
    return config;
}

/**
 * Helper to create an OAuth test client with mock server
 */
static std::unique_ptr<OAClient> makeOAuthTestClient(
    MockOAuth2Server& mockServer,
    const httpcl::Config& httpConfig,
    const std::string& tokenUrl = "https://auth.example.com/token",
    const std::string& refreshUrl = "",
    const std::vector<std::string>& scopes = {},
    std::function<httpcl::IHttpClient::Result(const std::string_view&, const httpcl::OptionalBodyAndContentType&, const httpcl::Config&)> customPostFun = nullptr)
{
    auto config = makeOAuth2Config(tokenUrl, refreshUrl, scopes);
    auto client = std::make_unique<httpcl::MockHttpClient>();
    
    if (customPostFun) {
        client->postFun = customPostFun;
    } else {
        // Default implementation that handles token/refresh requests and passes through API calls
        client->postFun = [&mockServer, tokenUrl, refreshUrl](auto uri, auto body, auto conf) {
            if (uri == tokenUrl) {
                return mockServer.handleTokenRequest(uri, body, conf);
            }
            if (!refreshUrl.empty() && uri == refreshUrl) {
                return mockServer.handleRefreshRequest(uri, body, conf);
            }
            // Default API response
            return httpcl::IHttpClient::Result{200, ""};
        };
    }
    
    return std::make_unique<OAClient>(config, std::move(client), httpConfig);
}

TEST_CASE("OAuth2 Client Credentials Flow", "[oauth2]") {
    MockOAuth2Server mockServer;
    
    // Create HTTP settings with OAuth2 config
    httpcl::Config httpConfig;
    httpConfig.oauth2 = httpcl::Config::OAuth2{"test-client", "test-secret"};
    
    SECTION("Successful Token Request") {
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig, "https://auth.example.com/token", "", {},
            [&](auto uri, auto body, auto conf) {
                if (uri == "https://auth.example.com/token") {
                    return mockServer.handleTokenRequest(uri, body, conf);
                }
                // API call after getting token
                REQUIRE(conf.headers.count("Authorization") > 0);
                auto authHeader = conf.headers.find("Authorization");
                REQUIRE(authHeader->second == "Bearer " + mockServer.lastIssuedAccessToken);
                return httpcl::IHttpClient::Result{200, ""};
            });
        
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        REQUIRE_NOTHROW(
            oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr)
        );
        
        REQUIRE(mockServer.tokenRequestCount == 1);
        REQUIRE(mockServer.lastIssuedAccessToken == "access_1");
    }
    
    SECTION("Token Caching") {
        mockServer.tokenExpirySeconds = 3600;
        
        int apiCallCount = 0;
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig, "https://auth.example.com/token", "", {},
            [&](auto uri, auto body, auto conf) {
                if (uri == "https://auth.example.com/token") {
                    return mockServer.handleTokenRequest(uri, body, conf);
                }
                apiCallCount++;
                return httpcl::IHttpClient::Result{200, ""};
            });
        
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        // First call should get token
        oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(mockServer.tokenRequestCount == 1);
        REQUIRE(apiCallCount == 1);
        
        // Second call should use cached token
        oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(mockServer.tokenRequestCount == 1); // No new token request
        REQUIRE(apiCallCount == 2);
    }
    
    SECTION("Token Refresh") {
        mockServer.tokenExpirySeconds = 1; // Very short expiry
        mockServer.shouldReturnRefreshToken = true;
        
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig, 
            "https://auth.example.com/token",
            "https://auth.example.com/refresh");
        
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        // First call gets initial token
        oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(mockServer.tokenRequestCount == 1);
        REQUIRE(mockServer.refreshRequestCount == 0);
        
        // Wait for token to expire
        std::this_thread::sleep_for(2s);
        
        // Next call should trigger refresh
        oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(mockServer.tokenRequestCount == 1);
        REQUIRE(mockServer.refreshRequestCount == 1);
        REQUIRE(mockServer.lastIssuedAccessToken.find("refreshed_") == 0);
    }
    
    SECTION("Scopes Handling") {
        std::vector<std::string> requestedScopes = {"read", "write"};
        
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig, 
            "https://auth.example.com/token", "", requestedScopes);
        
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        
        // Verify scopes were sent in the request
        REQUIRE((mockServer.lastTokenRequestBody.find("scope=read%20write") != std::string::npos ||
                mockServer.lastTokenRequestBody.find("scope=read+write") != std::string::npos));
    }
    
    SECTION("Audience Parameter") {
        mockServer.expectedAudience = "https://api.example.com";
        httpConfig.oauth2->audience = "https://api.example.com";
        
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig);
        
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        
        // Verify audience was sent
        REQUIRE(mockServer.lastTokenRequestBody.find("audience=") != std::string::npos);
    }
    
    SECTION("Error Handling - Invalid Credentials") {
        mockServer.shouldFailTokenRequest = true;
        
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig);
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        REQUIRE_THROWS_AS(
            oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr),
            std::runtime_error
        );
    }
    
    SECTION("Error Handling - Missing OAuth2 Config") {
        httpcl::Config emptyHttpConfig; // No OAuth2 config
        
        auto oaClient = makeOAuthTestClient(mockServer, emptyHttpConfig);
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        REQUIRE_THROWS_WITH(
            oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr),
            Catch::Matchers::ContainsSubstring("OAuth2 client-credentials required but no oauth2 config present")
        );
    }
    
    SECTION("Error Handling - Invalid Token Response") {
        mockServer.shouldReturnInvalidResponse = true;
        
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig);
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        REQUIRE_THROWS_WITH(
            oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr),
            Catch::Matchers::ContainsSubstring("access_token missing")
        );
    }
    
    SECTION("Public Client (No Secret)") {
        httpcl::Config publicClientConfig;
        publicClientConfig.oauth2 = httpcl::Config::OAuth2{"test-client"};
        
        auto oaClient = makeOAuthTestClient(mockServer, publicClientConfig, "https://auth.example.com/token", "", {},
            [&](auto uri, auto body, auto conf) {
                if (uri == "https://auth.example.com/token") {
                    // Verify client_id is in body for public clients
                    REQUIRE(body->body.find("client_id=test-client") != std::string::npos);
                    return mockServer.handleTokenRequest(uri, body, conf);
                }
                return httpcl::IHttpClient::Result{200, ""};
            });
        
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        REQUIRE_NOTHROW(
            oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr)
        );
    }
    
    SECTION("URL Override from HTTP Settings") {
        httpConfig.oauth2->tokenUrlOverride = "https://override.example.com/token";
        httpConfig.oauth2->refreshUrlOverride = "https://override.example.com/refresh";
        
        // Note: Using original URL in config, but it should be overridden
        auto oaClient = makeOAuthTestClient(mockServer, httpConfig, "https://original.example.com/token", "", {},
            [&](auto uri, auto body, auto conf) {
                // API Request can just pass
                if (uri == "https://api.example.com/test") {
                    return httpcl::IHttpClient::Result{200, "OK"};
                }
                // Token request should use override URL, not original
                REQUIRE(uri == "https://override.example.com/token");
                return mockServer.handleTokenRequest(uri, body, conf);
            });
        
        auto request = service_client_test::Request("test", 0, {}, service_client_test::Flat("", ""));
        
        oaClient->callMethod("test", zserio::ReflectableServiceData(request.reflectable()), nullptr);
    }
}

