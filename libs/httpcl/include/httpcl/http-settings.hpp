#pragma once

#include <optional>
#include <map>
#include <vector>
#include <string>
#include <shared_mutex>
#include <atomic>
#include <deque>
#include <chrono>
#include <regex>

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

    /** Parse one YAML entry, expanding $env.NAME in string values; invalid values throw. */
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

    // scope uses escaped prefix matching with * wildcards; urlPatternString may
    // instead hold the raw regex from a YAML url entry. Keys are not expanded.
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
     * Merge another matching entry into this request configuration. Headers and
     * query parameters append (including duplicate names); existing cookie names
     * win. Present auth/proxy/API-key values replace previous values. Non-empty
     * OAuth fields override individually, and useForSpecFetch always overrides.
     */
    Config& operator |= (Config const& other);

    /**
     * Convert this configuration to YAML for Config(yamlConf), including cleartext
     * secrets. Repeated header/query names collapse to one value; this is not a lossless round trip
     * for the multimap fields or a secret-redacting export.
     */
    std::string toYaml() const;

    /**
     * Create a human-readable summary for logging, with partial credential redaction.
     * Passwords, client secrets and exactly named Authorization headers are masked; long API
     * keys retain their first/last four characters. Other headers, query parameters
     * and cookies are printed verbatim, so this is not a general secret scrubber.
     */
    std::string toSafeString() const;
};

/**
 * Load/store settings from/to HTTP_SETTINGS_FILE and combine entries matching
 * each request URL.
 * Lookups lazily reload after updateTimestamp; no filesystem watcher is installed.
 * Lookup/load synchronize internally. Direct edits through the public containers,
 * getOrCreateConfigScope and store require external synchronization.
 */
struct Settings
{
    Settings();

    /** Clear current entries and parse the configured file; parse failures may leave a valid
     * prefix. */
    void load();

    /** Write the current settings array to HTTP_SETTINGS_FILE; callers serialize edits and writes.
     */
    void store();

    /**
     * Return an independent aggregate of all matching entries in file order,
     * using Config's merge rules. Reload first when the shared timestamp advances.
     */
    Config operator[](const std::string& url) const;

    /**
     * Get or create a Config entry for an exact scope and return a reference for editing.
     * The caller must exclude concurrent lookups/reloads and must not retain
     * the reference across load().
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
     * Read the password for a service/user pair from the system keychain.
     * A timeout returns an empty string; lookup failures and disabled keychain
     * support throw. The result is the secret value, not a keychain reference.
     * Timeout is not a hard deadline: destruction still joins the async task.
     */
    static std::string load(
        const std::string& service,
        const std::string& user);

    /**
     * Store a password under service/user, generating a service name when empty.
     * Returns that name, or an empty string on timeout. Disabled support throws.
     * The asynchronous task's errors are not retrieved by this wrapper.
     */
    static std::string store(
        const std::string& service,
        const std::string& user,
        const std::string& password);

    /**
     * Delete a keychain password and return the platform error object's boolean
     * conversion. False also covers timeout; disabled keychain support throws.
     * This return value is not a success acknowledgement.
     */
    static bool remove(
        const std::string& service,
        const std::string& user);
};

}
