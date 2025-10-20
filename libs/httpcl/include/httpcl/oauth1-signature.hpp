#pragma once

#include <string>
#include <map>

namespace httpcl
{

/**
 * OAuth 1.0 signature utilities for RFC 5849.
 * Supports HMAC-SHA256 signature method only.
 */
namespace oauth1
{

/**
 * Generate a cryptographically secure random nonce.
 * @param length Number of characters (8-64)
 * @return Alphanumeric nonce string
 */
std::string generateNonce(int length = 16);

/**
 * Generate OAuth 1.0 timestamp (seconds since epoch).
 * @return Timestamp as string
 */
std::string generateTimestamp();

/**
 * Compute OAuth 1.0 signature using HMAC-SHA256.
 *
 * @param httpMethod HTTP method (e.g., "POST")
 * @param url Full URL (without query string)
 * @param params All parameters (OAuth params + body params)
 * @param consumerSecret Client secret for signing
 * @param tokenSecret Token secret (empty for client credentials flow)
 * @return Base64-encoded signature
 */
std::string computeSignature(
    const std::string& httpMethod,
    const std::string& url,
    const std::map<std::string, std::string>& params,
    const std::string& consumerSecret,
    const std::string& tokenSecret = "");

/**
 * Build complete OAuth 1.0 Authorization header.
 *
 * @param httpMethod HTTP method (e.g., "POST")
 * @param url Full URL (without query string)
 * @param consumerKey Client ID
 * @param consumerSecret Client secret
 * @param bodyParams Parameters from request body (for signature)
 * @param nonceLength Length of generated nonce (8-64)
 * @return Authorization header value (e.g., "OAuth oauth_consumer_key=...")
 */
std::string buildAuthorizationHeader(
    const std::string& httpMethod,
    const std::string& url,
    const std::string& consumerKey,
    const std::string& consumerSecret,
    const std::map<std::string, std::string>& bodyParams = {},
    int nonceLength = 16);

}  // namespace oauth1
}  // namespace httpcl
