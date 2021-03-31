#pragma once

#include <httplib.h>
#include <optional>
#include <map>
#include <vector>
#include <string>

namespace httpcl
{

/**
 * Loads AFW_HTTP_SETTINGS_FILE.
 * Allows returning settings for a specific URL.
 */
struct HTTPSettings
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

    struct Settings {
        std::map<std::string, std::string> cookies;
        std::optional<BasicAuthentication> auth;
        std::optional<Proxy> proxy;
    };

    HTTPSettings();

    void load();
    void store();

    /**
     * Apply matching (url) settings to http-client instance.
     * May read keychain passwords which can block and require user interaction.
     */
    void apply(const std::string& url, httplib::Client& client);

    /**
     * Return settings entry with url pattern, or null if none was found.
     */
    Settings* find(const std::string& urlPattern);

    /**
     * Read password from system keychain.
     * Returns keychain service string.
     */
    static std::string loadPassword(const std::string& service,
                                    const std::string& user);

    /**
     * Store password into system keychain.
     * Returns the generated keychain service string to be set as `keychain`.
     */
    static std::string storePassword(const std::string& service,
                                     const std::string& user,
                                     const std::string& password);

    /**
     * Delete keychain password.
     * Returns `true` on success.
     */
    static bool deletePassword(const std::string& service,
                               const std::string& user);

    std::map<std::string, Settings> settings; /* url-pattern -> settings */
};

}
