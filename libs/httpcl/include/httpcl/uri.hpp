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

/** Mutable URI parts. Parsing discards user info/fragments; build is not a lossless round trip. */
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
     * Split an absolute URI with scheme/authority into stored parts. Path and
     * query percent escapes are decoded, but query pairs are not split into
     * queryVars. User info and fragments are discarded. Unsupported syntax
     * raises URIError; this helper is not a general URI validator.
     * See https://tools.ietf.org/html/rfc3986 for the URI syntax.
     */
    static URIComponents fromStrRfc3986(std::string const& uriString);

    /**
     * Extract an empty or slash-prefixed path and optional query. No leading
     * scheme, host or port info may be present. Escapes are decoded and any
     * fragment is discarded, as with fromStrRfc3986.
     *
     * Throws URIError.
     */
    static URIComponents fromStrPath(std::string const& pathAndQueryString);

    /**
     * Resolve the reference forms used in OpenAPI servers[].url:
     * - Absolute URI: https://example.com/v1 is parsed independently.
     * - Server-relative path: /v1 replaces the base path.
     * - Document-relative path: ., ./v2, ../v2 and v2 merge with the base
     *   directory and remove dot segments (RFC 3986 section 5.2.4).
     * Relative references retain the base scheme, host and port and take their
     * own query. Use a path-bearing reference: network-path (//host), query-only
     * and fragment references do not receive general RFC resolution semantics.
     *
     * An empty reference returns base unchanged. Other relative references
     * require a base scheme and host, otherwise URIError is thrown.
     */
    static URIComponents resolveReference(std::string const& reference,
                                          URIComponents const& base);

    /**
     * Append slash-separated path segments, encoding each with encode().
     * Repeated interior separators are collapsed, while an empty input or
     * trailing separator produces a final slash. Already percent-encoded input
     * is encoded again.
     */
    void appendPath(const std::string& part);

    /**
     * Append a query pair; duplicate keys are retained and emitted in key order.
     */
    void addQuery(std::string key, std::string value);

    /**
     * Build the final URI string by joining buildHost() and buildPath().
     * A missing scheme or host raises URIError.
     * The stored path is used as-is, while query text and queryVars are encoded.
     */
    std::string build() const; /* Full URI */
    std::string buildPath() const; /* URI path + query */
    std::string buildHost() const; /* Scheme + host + nonzero port */

    /**
     * Encode bytes except unreserved characters and URI sub-delimiters.
     * In particular & and = remain literal; use encodeComponent for an isolated
     * value that must not introduce form/query separators.
     */
    static std::string encode(std::string str);

    /**
     * Encode a single URI component. Unlike encode(), this only leaves RFC 3986
     * unreserved characters untouched and is suitable for form-body values.
     */
    static std::string encodeComponent(std::string str);

    /**
     * Decode valid percent-encoded octets. Invalid escapes and plus signs are
     * retained unchanged; this is not form-urlencoded plus-to-space decoding.
     */
    static std::string decodeComponent(std::string str);
};

}
