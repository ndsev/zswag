#pragma once

#include <httplib.h>
#include <optional>
#include <map>
#include <vector>
#include <string>
#include <shared_mutex>
#include <atomic>
#include <deque>

#include "yaml-cpp/yaml.h"


namespace httpcl
{

using Headers = std::multimap<std::string, std::string>;
using Query = std::multimap<std::string, std::string>;

/**
 * Set of configs for an HTTP connection, including:
 *   - Extra Headers
 *   - Extra Query Parameters
 *   - Extra Cookies
 *   - Optional Proxy-Config
 *   - Optional Basic-Auth
 *   - API-Key
 */
struct Config
{
    Config() = default;
    Config(std::string const& yamlConf);

    struct BasicAuthentication {
        std::string user;
        std::string password;
        std::string keychain;
    };

    struct Proxy {
        std::string host;
        int port = 0;
        std::string user;
        std::string password;
        std::string keychain;
    };

    struct OAuth2 {
        std::string clientId;
        std::string clientSecret;
        std::string clientSecretKeychain;
        std::string tokenUrlOverride; // optional
        std::string refreshUrlOverride; // optional
        std::string audience; // optional
        std::vector<std::string> scopesOverride; // optional
        bool useForSpecFetch = true;  // Use OAuth2 token when fetching OpenAPI spec (default: true)

        /**
         * Token endpoint authentication method.
         * Specifies how the client authenticates when requesting tokens.
         */
        enum class TokenEndpointAuthMethod {
            Rfc6749_ClientSecretBasic,  // client_secret_basic (RFC 6749 Section 2.3.1)
            Rfc5849_Oauth1Signature     // OAuth 1.0 HMAC-SHA256 signature (RFC 5849)
        };

        /**
         * Configuration for token endpoint authentication.
         */
        struct TokenEndpointAuth {
            TokenEndpointAuthMethod method = TokenEndpointAuthMethod::Rfc6749_ClientSecretBasic;
            int nonceLength = 16;  // For Rfc5849_Oauth1Signature: nonce length (8-64)
        };

        std::optional<TokenEndpointAuth> tokenEndpointAuth;

        /**
         * Helper to get auth method with default.
         * Returns Rfc6749_ClientSecretBasic if tokenEndpointAuth is not configured.
         */
        TokenEndpointAuthMethod getTokenEndpointAuthMethod() const {
            return tokenEndpointAuth ? tokenEndpointAuth->method : TokenEndpointAuthMethod::Rfc6749_ClientSecretBasic;
        }
    };

    std::optional<std::string> scope;
    std::regex urlPattern;
    std::string urlPatternString;

    std::map<std::string, std::string> cookies;
    std::optional<BasicAuthentication> auth;
    std::optional<Proxy> proxy;
    std::optional<OAuth2> oauth2;
    std::optional<std::string> apiKey;
    Headers headers;
    Query query;

    /**
     * Merge this configuration with another.
     */
    Config& operator |= (Config const& other);

    /**
     * Apply this configuration to an httplib client.
     * May read keychain passwords which can block and require user interaction.
     */
    void apply(httplib::Client& cl) const;

    /**
     * Convert this configuration to a YAML string, which may
     * be passed to the respective `Config(yamlConf)` constructor.
     */
    std::string toYaml() const;

    /**
     * Create a human-readable summary of this configuration for logging,
     * with sensitive values (passwords, secrets, API keys, static bearer tokens) masked.
     * OAuth2 fetched tokens are NOT masked as they are temporary.
     */
    std::string toSafeString() const;
};

/**
 * Loads/stores settings from/to HTTP_SETTINGS_FILE.
 * Allows returning config for a specific URL.
 */
struct Settings
{
    Settings();

    void load();
    void store();

    /**
     * Get aggregated configuration for the given URL.
     */
    Config operator[](const std::string& url) const;

    /**
     * Get or create a Config entry by a target scope.
     */
    Config& getOrCreateConfigScope(std::string_view const& scope);

    /**
     * Map from URL pattern to some config values.
     */
    std::deque<Config> settings;
    YAML::Node document;
    mutable std::shared_mutex mutex;
    std::chrono::steady_clock::time_point lastRead;

    /**
     * Prompt settings instance to re-parse the HTTP settings file,
     * by calling updateTimestamp with std::chrono::steady_clock::now().
     */
    static void updateTimestamp(std::chrono::steady_clock::time_point time);
    static std::atomic<std::chrono::steady_clock::time_point> lastUpdated;
};

struct secret
{
    /**
     * Read password from system keychain.
     * Returns keychain service string.
     */
    static std::string load(
        const std::string& service,
        const std::string& user);

    /**
     * Store password into system keychain.
     * Returns the generated keychain service string to be set as `keychain`.
     */
    static std::string store(
        const std::string& service,
        const std::string& user,
        const std::string& password);

    /**
     * Delete keychain password.
     * Returns `true` on success.
     */
    static bool remove(
        const std::string& service,
        const std::string& user);
};

}
