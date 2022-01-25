#include "uri.hpp"

#include <iostream>
#include <cstring>
#include <cctype>

#include "httpcl/log.hpp"
#include "stx/format.h"

namespace httpcl
{

/**
 * Reserved characters.
 *
 * https://tools.ietf.org/html/rfc3986#section-2.2
 */

static auto isPctEncoded(int c)
{
    return c == '%' || std::isxdigit(c);
}

static auto isUnreserved(int c)
{
    return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

static auto isSubDelim(int c)
{
    return c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' ||
        c == ')' || c == '*' || c == '+' || c == ',' || c == ';' || c == '=';
}

static auto isPChar(int c)
{
    return isUnreserved(c) || isPctEncoded(c) || isSubDelim(c) ||
        c == ':' || c == '@';
}

/**
 * Parse scheme
 *
 * https://tools.ietf.org/html/rfc3986#section-3.1
 */
static auto parseScheme(const char*& str, std::string* out)
{
    if (std::isalpha(*str)) {
        if (out)
            out->push_back(*str);
        ++str;
        while (std::isalnum(*str) ||
               *str == '-' ||
               *str == '+' ||
               *str == '.') {
            if (out)
                out->push_back(*str);
            ++str;
        }
    } else
        return false;

    if (*str++ != ':')
        return false;

    return true;
}

/**
 * Parse authority + port.
 *
 * https://tools.ietf.org/html/rfc3986#section-3.2
 */
static auto parseAuthority(const char*& str,
                           std::string* user,
                           std::string* host,
                           uint16_t* port)
{
    if (str[0] != '/' && str[1] != '/')
        return false;
    str += 2;

    /* User Information */
    auto userEnd = std::strchr(str, '@');
    if (userEnd) {
        auto afterChr = [&](auto c) {
            auto pos = std::strchr(str, c);
            return pos && userEnd > pos;
        };

        /* Make sure not to skip into path, query or fragment. */
        if (!afterChr('/') && !afterChr('?') && !afterChr('#')) {
            if (user)
                user->assign(str, userEnd);
            str = userEnd + 1;
        }
    }

    /* IP-Literal */
    if (*str == '[') {
        if (host)
            host->push_back('[');
        ++str;

        /* IPvFuture prefix */
        if (str[0] == 'v' && std::isxdigit(str[1]) && str[2] == '.') {
            if (host) {
                host->push_back(*str++);
                host->push_back(*str++);
                host->push_back(*str++);
            }
        }

        /* Allowed IPv4/IPv6 characters */
        while (std::isxdigit(*str) || *str == ':' || *str == '.') {
            if (host)
                host->push_back(*str);
            ++str;
        }

        if (*str != ']')
            return false;

        if (host)
            host->push_back(']');
        ++str;
    }

    /* IPv4 & Reg-Name */
    while (std::isalnum(*str) ||
        *str == '-' ||
        *str == '.' ||
        *str == '_' ||
        *str == '~') {
        if (host)
            host->push_back(*str);
        ++str;
    }

    /* Port */
    if (*str == ':') {
        ++str;
        while (std::isdigit(*str)) {
            if (port)
                *port = (*port * 10u) + (*str - '0');
            ++str;
        }
    }

    return true;
}

static auto decodePctEncoded(const char*& str, std::string* out)
{
    if (*str == '%') {
        if (std::isxdigit(str[1]) && std::isxdigit(str[2])) {
            const char hex[3] = {
                str[1], str[2], '\0'
            };

            if (out)
                out->push_back((char)std::strtol(hex, nullptr, 16));
            str += 3;
        } else
            ++str;
    }
}

/**
 * Parse path.
 *
 * https://tools.ietf.org/html/rfc3986#section-3.3
 */
static auto parsePath(const char*& str, std::string* path)
{
    if (*str == '/') {
        if (path)
            path->push_back(*str);
        ++str;

        while (isPChar(*str) || *str == '/') {
            if (*str == '%') {
                decodePctEncoded(str, path);
            } else {
                if (path)
                    path->push_back(*str);
                ++str;
            }
        }
    }

    /* Path must end with either EOF, '?' or '#' */
    if (*str == '\0' || *str == '?' || *str == '#')
        return true;

    return false;
}

/**
 * Parse query.
 *
 * https://tools.ietf.org/html/rfc3986#section-3.4
 */
static auto parseQuery(const char*& str, std::string* query)
{
    while (isPChar(*str)) {
        if (*str == '%') {
            decodePctEncoded(str, query);
        } else {
            if (query)
                query->push_back(*str);
            ++str;
        }
    }

    /* Query must end with either EOF or '#' (fragment indicator) */
    if (*str == '\0' || *str == '#')
        return true;

    return false;
}

URIComponents URIComponents::fromStrRfc3986(std::string const& uri)
{
    URIComponents result;
    const auto* c = uri.c_str();
    std::string error;

    if (!parseScheme(c, &result.scheme)) error = "Error parsing scheme";
    if (!parseAuthority(c, nullptr, &result.host, &result.port)) error = "Error parsing authority";
    if (!parsePath(c, &result.path)) error = "Error parsing path";
    if (*c == '?' && !parseQuery(++c, &result.query)) error = "Error parsing query";

    if (!error.empty()) {
        throw logRuntimeError<URIError>(stx::format("[URIComponents::fromStrRfc3986] {} of URI '{}'", error, uri));
    }

    return result;
}

URIComponents URIComponents::fromStrPath(std::string const& pathAndQueryString) {
    URIComponents result;
    const auto* c = pathAndQueryString.c_str();

    if (!parsePath(c, &result.path))
        throw logRuntimeError<URIError>(
            stx::format("[URIComponents::fromStrPath] Error parsing path from '{}'", pathAndQueryString));

    if (*c == '?')
        if (!parseQuery(++c, &result.query))
            throw logRuntimeError<URIError>(
                stx::format("[URIComponents::fromStrPath] Error parsing query from '{}'", pathAndQueryString));

    return result;
}

URIComponents::URIComponents(
    std::string scheme,
    std::string host,
    std::string const& path,
    uint16_t port,
    std::string query
) :
    scheme(std::move(scheme)),
    host(std::move(host)),
    port(port),
    query(std::move(query))
{
    appendPath(path);
}

void URIComponents::appendPath(const std::string& part)
{
    std::string::size_type partBegin = 0u;
    for (;;) {
        auto partEnd = part.find('/', partBegin);
        auto partLength = partEnd == std::string::npos
            ? std::string::npos
            : partEnd - partBegin;

        if (partLength == 0) {
            ++partBegin;
            continue;
        }

        if (path.empty() || path.back() != '/')
            path.push_back('/');
        path += encode(part.substr(partBegin, partLength));
        if (partEnd == std::string::npos)
            break;

        partBegin = partEnd + 1u;
    }
}

void URIComponents::addQuery(std::string key, std::string value)
{
    queryVars.insert(std::make_pair(std::move(key),
                                    std::move(value)));
}

std::string URIComponents::build() const
{
    return buildHost() + buildPath();
}

std::string URIComponents::buildHost() const
{
    if (scheme.empty())
        throw logRuntimeError<URIError>("[URIComponents::buildHost] Missing scheme");

    if (host.empty())
        throw logRuntimeError<URIError>("[URIComponents::buildHost] Missing host");

    return scheme + "://" +
           host +
           (port > 0 ? std::string(":") + std::to_string(port) : "");
}

std::string URIComponents::buildPath() const
{
    std::string uri = path;

    std::string queryStr = (query.empty() ? "" : "?") + encode(query);
    for (const auto& queryPair : queryVars) {
        queryStr.push_back(queryStr.empty() ? '?': '&');
        queryStr += encode(queryPair.first) + "=" +
            encode(queryPair.second);
    }

    if (!queryStr.empty())
        uri += queryStr;

    return uri;
}

std::string URIComponents::encode(std::string str)
{
    static const auto alpha =
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "-._~"        /* unreserved */
        "!$&'()*+,;=" /* sub-delims */;

    for (std::string::size_type i = 0;;) {
        i = str.find_first_not_of(alpha, i);
        if (i == std::string::npos)
            break;

        const char codepoint = str[i];
        char hex[3 + 1] = {}; /* %XX + \0 */

        auto len = snprintf(hex, sizeof(hex), "%%%02x",
                            *((unsigned char*)&codepoint) & 0xff);
        if (len > 0) {
            str.replace(i, 1, hex);
            i += strlen(hex);
        } else {
            ++i;
        }
    }

    return str;
}

}
