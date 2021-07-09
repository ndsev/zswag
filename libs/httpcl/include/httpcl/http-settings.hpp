#pragma once

#include <httplib.h>
#include <optional>
#include <map>
#include <vector>
#include <string>


namespace httpcl
{

using Headers = std::multimap<std::string, std::string>;
using Query = std::multimap<std::string, std::string>;
using HeadersAndQuery = std::pair<Headers, Query>;

/**
 *
 */
struct Config
{
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

    std::map<std::string, std::string> cookies;
    std::optional<BasicAuthentication> auth;
    std::optional<Proxy> proxy;
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
};

/**
 * Loads settings from HTTP_SETTINGS_FILE.
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
     * Map from URL pattern to some config values.
     */
    std::map<std::string, Config> settings;
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
