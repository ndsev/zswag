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

    std::optional<std::string> scope;
    std::regex urlPattern;
    std::string urlPatternString;

    std::map<std::string, std::string> cookies;
    std::optional<BasicAuthentication> auth;
    std::optional<Proxy> proxy;
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
