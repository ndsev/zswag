#pragma once

#include <string>
#include <map>
#include <stdexcept>
#include <cstdint>

namespace httpcl
{

struct URIError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct URIComponents
{
    std::string scheme;
    std::string host;
    std::string path;
    std::uint16_t port = 0u;
    std::string query;
    std::multimap<std::string, std::string> queryVars;

    URIComponents() = default;
    URIComponents(std::string scheme,
                  std::string host,
                  std::string const& path,
                  uint16_t port,
                  std::string query);

    /**
     * Split RFC3986 URI into parts.
     *
     * See https://tools.ietf.org/html/rfc3986
     *
     * Throws URIError.
     */
    static URIComponents fromStrRfc3986(std::string const& uriString);

    /**
     * Extract the path and query URI components from a string. No leading
     * scheme, host or port info must be present.
     *
     * Throws URIError.
     */
    static URIComponents fromStrPath(std::string const& pathAndQueryString);

    /**
     * Append one or multiple path-parts ("/a/b/c") to the URIs
     * path.
     */
    void appendPath(const std::string& part);

    /**
     * Add a query-var key-value pair.
     */
    void addQuery(std::string key, std::string value);

    /**
     * Build the final URI string.
     *
     * Throws URIError.
     */
    std::string build() const; /* Full URI */
    std::string buildPath() const; /* URI path + query */
    std::string buildHost() const; /* Scheme + host */

    /**
     * Helper function for URL encoding a string.
     */
    static std::string encode(std::string str);
};

}
