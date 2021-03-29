// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.

#include "http-settings.hpp"

#include <keychain/keychain.h>
#include <yaml-cpp/yaml.h>

#include <iostream>
#include <cstdlib>
#include <regex>
#include <future>

using namespace httpcl;
using namespace std::string_literals;

static const std::chrono::minutes KEYCHAIN_TIMEOUT{1};
static const char* KEYCHAIN_PACKAGE{"org.nds.live"};

namespace YAML
{
template <>
struct convert<HTTPSettings::BasicAuthentication>
{
    static Node encode(const HTTPSettings::BasicAuthentication& a)
    {
        Node node;
        node["user"] = a.user;
        if (!a.password.empty())
            node["password"] = a.password;
        else if (!a.keychain.empty())
            node["keychain"] = a.keychain;

        return node;
    }

    static bool decode(const Node& node, HTTPSettings::BasicAuthentication& a)
    {
        if (!node.IsMap())
            return false;

        const auto& user = node["user"];
        const auto& password = node["password"];
        const auto& keychain = node["keychain"];

        if (!user)
            return false;

        a.user = user.as<std::string>();

        if (password)
            a.password = password.as<std::string>();
        else if (keychain)
            a.keychain = keychain.as<std::string>();
        else
            return false;

        return true;
    }
};

template <>
struct convert<HTTPSettings::Proxy>
{
    static Node encode(const HTTPSettings::Proxy& a)
    {
        Node node;
        node["host"] = a.host;
        node["port"] = a.port;

        if (!a.user.empty()) {
            node["user"] = a.user;
            if (!a.password.empty())
                node["password"] = a.password;
            else if (!a.keychain.empty())
                node["keychain"] = a.keychain;
        }

        return node;
    }

    static bool decode(const Node& node, HTTPSettings::Proxy& a)
    {
        const auto& host = node["host"];
        const auto& port = node["port"];

        if (!host || !port)
            return false;

        a.host = host.as<std::string>();
        a.port = port.as<int>();

        const auto& user = node["user"];
        const auto& password = node["password"];
        const auto& keychain = node["keychain"];

        if (user) {
            a.user = user.as<std::string>();

            if (password)
                a.password = password.as<std::string>();
            else if (keychain)
                a.keychain = keychain.as<std::string>();
            else
                return false;
        }

        return true;
    }
};
}

HTTPSettings::HTTPSettings()
{
    load();
}

void HTTPSettings::load()
{
    settings.clear();

    auto cookieJar = std::getenv("AFW_HTTP_SETTINGS_FILE");
    if (!cookieJar)
        return;

    try {
        auto node = YAML::LoadFile(cookieJar);
        uint32_t idx = 0;

        for (auto const& entry : node.as<std::vector<YAML::Node>>()) {
            Settings settings;
            std::string urlPattern;

            if (auto entryParam = entry["url"])
                urlPattern = entryParam.as<std::string>();
            else
                throw std::runtime_error(
                    "HTTPSettings: Failed to read 'url' of entry #"s + std::to_string(idx) +
                    " in " + cookieJar);

            if (auto cookies = entry["cookies"])
                settings.cookies = cookies.as<std::map<std::string, std::string>>();

            if (auto basicAuth = entry["basic-auth"])
                settings.auth = basicAuth.as<HTTPSettings::BasicAuthentication>();

            if (auto proxy = entry["proxy"])
                settings.proxy = proxy.as<HTTPSettings::Proxy>();

            this->settings[urlPattern] = std::move(settings);
            ++idx;
        }
    } catch (const YAML::BadFile&) {
        /* Ignore: Could not read file. */
    } catch (const std::exception& e) {
        std::cerr << "Failed to read http-settings from '"
                  << cookieJar << "': " << e.what() << std::endl;
    }
}

void HTTPSettings::store()
{
    auto cookieJar = std::getenv("AFW_HTTP_SETTINGS_FILE");
    if (!cookieJar)
        return;

    try {
        auto node = YAML::Node();

        for (const auto& pair : settings) {
            auto settingsNode = YAML::Node();

            settingsNode["url"] = pair.first;
            const auto& entry = pair.second;

            if (!entry.cookies.empty())
                settingsNode["cookies"] = entry.cookies;

            if (const auto& auth = entry.auth)
                settingsNode["basic-auth"] = *auth;

            if (const auto& proxy = entry.proxy)
                settingsNode["proxy"] = *proxy;

            node.push_back(std::move(settingsNode));
        }

        std::ofstream os(cookieJar);
        os << node;
    } catch (const std::exception& e) {
        std::cerr << "Failed to write http-settings to '"
                  << cookieJar << "': " << e.what() << "\n";
    }
}

void HTTPSettings::apply(std::string const& url, httplib::Client& client)
{
    httplib::Headers headers;

    for (auto const& pair : settings) {
        if (!std::regex_match(url, std::regex(pair.first)))
            continue;

        const auto& entry = pair.second;

        /* Cookies */
        std::string cookieHeaderValue;
        for (const auto& cookie : entry.cookies) {
            if (!cookieHeaderValue.empty())
                cookieHeaderValue += "; ";
            cookieHeaderValue += cookie.first + "=" + cookie.second;
        }

        if (!cookieHeaderValue.empty())
            headers.insert({"Cookie", cookieHeaderValue});

        /* Basic Authentication */
        if (const auto& auth = entry.auth) {
            auto password = auth->password;
            if (!auth->keychain.empty()) {
                password = loadPassword(auth->keychain,
                                        auth->user);
            }

            headers.insert(httplib::make_basic_authentication_header(auth->user.c_str(),
                                                                     password.c_str()));
        }

        /* Proxy Settings */
        if (const auto& proxy = entry.proxy) {
            client.set_proxy(proxy->host.c_str(), proxy->port);

            auto password = proxy->password;
            if (!proxy->keychain.empty())
                password = loadPassword(proxy->keychain,
                                        proxy->user);

            if (!proxy->user.empty())
                client.set_proxy_basic_auth(proxy->user.c_str(),
                                            password.c_str());
        }
    }

    client.set_default_headers(std::move(headers));
}

std::string HTTPSettings::loadPassword(const std::string& service,
                                       const std::string& user)
{
    auto result = std::async(std::launch::async, [=]() {
        keychain::Error error;
        auto password = keychain::getPassword(KEYCHAIN_PACKAGE,
                                              service,
                                              user,
                                              error);

        if (error)
            throw std::runtime_error(error.message);
        return password;
    });

    if (result.wait_for(KEYCHAIN_TIMEOUT) == std::future_status::timeout)
        return {};

    return result.get();
}

std::string HTTPSettings::storePassword(const std::string& service,
                                        const std::string& user,
                                        const std::string& password)
{
    auto randServiceId = []() {
        std::string id(12, '.');
        std::generate(id.begin(), id.end(), []() {
            return "0123456789abcdef"[rand() % 16];
        });
        return id;
    };

    auto newService = service.empty()
        ? "service password "s + randServiceId()
        : service;

    auto result = std::async(std::launch::async, [=]() {
        keychain::Error error;
        keychain::setPassword(KEYCHAIN_PACKAGE,
                              newService,
                              user,
                              password,
                              error);

        if (error)
            throw std::runtime_error(error.message);
    });

    if (result.wait_for(KEYCHAIN_TIMEOUT) == std::future_status::timeout)
        return {};

    return newService;
}

bool HTTPSettings::deletePassword(const std::string& service,
                                  const std::string& user)
{
    auto result = std::async(std::launch::async, [=]() {
        keychain::Error error;
        keychain::deletePassword(KEYCHAIN_PACKAGE,
                                 service,
                                 user,
                                 error);

        return error;
    });

    if (result.wait_for(KEYCHAIN_TIMEOUT) == std::future_status::timeout)
        return false;

    return result.get();
}
