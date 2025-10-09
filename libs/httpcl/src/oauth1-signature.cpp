#include "httpcl/oauth1-signature.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>
#include <httplib.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace httpcl
{
namespace oauth1
{

std::string generateNonce(int length)
{
    if (length < 8 || length > 64) {
        throw std::runtime_error("Nonce length must be between 8 and 64");
    }

    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    std::string nonce;
    nonce.reserve(length);
    for (int i = 0; i < length; ++i) {
        nonce += alphanum[dis(gen)];
    }
    return nonce;
}

std::string generateTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
    return std::to_string(seconds.count());
}

/**
 * RFC 3986 percent encoding.
 * OAuth 1.0 requires unreserved characters (A-Z, a-z, 0-9, -, ., _, ~) to NOT be encoded.
 * All other characters MUST be encoded.
 */
static std::string percentEncode(const std::string& input)
{
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex << std::uppercase;

    for (unsigned char c : input) {
        // RFC 3986 unreserved characters
        if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            encoded << c;
        } else {
            // Percent-encode everything else
            encoded << '%' << std::setw(2) << static_cast<int>(c);
        }
    }

    return encoded.str();
}

/**
 * Base64 encode using OpenSSL.
 */
static std::string base64Encode(const unsigned char* input, size_t length)
{
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    BIO_write(bio, input, static_cast<int>(length));
    BIO_flush(bio);

    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

/**
 * Compute HMAC-SHA256.
 */
static std::string hmacSha256(const std::string& key, const std::string& data)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &digestLen);

    return std::string(reinterpret_cast<char*>(digest), digestLen);
}

/**
 * Build signature base string according to RFC 5849 Section 3.4.1.
 */
static std::string buildSignatureBaseString(
    const std::string& httpMethod,
    const std::string& url,
    const std::map<std::string, std::string>& params)
{
    // 1. Normalize parameters: sort and concatenate
    std::vector<std::string> encodedPairs;
    for (const auto& [key, value] : params) {
        encodedPairs.push_back(percentEncode(key) + "=" + percentEncode(value));
    }
    std::sort(encodedPairs.begin(), encodedPairs.end());

    std::string paramString;
    for (size_t i = 0; i < encodedPairs.size(); ++i) {
        if (i > 0) paramString += "&";
        paramString += encodedPairs[i];
    }

    // 2. Build signature base string: METHOD&URL&PARAMS
    std::string baseString;
    baseString += httpMethod;
    baseString += "&";
    baseString += percentEncode(url);
    baseString += "&";
    baseString += percentEncode(paramString);

    return baseString;
}

std::string computeSignature(
    const std::string& httpMethod,
    const std::string& url,
    const std::map<std::string, std::string>& params,
    const std::string& consumerSecret,
    const std::string& tokenSecret)
{
    // Build signature base string
    std::string baseString = buildSignatureBaseString(httpMethod, url, params);

    // Build signing key: consumer_secret&token_secret
    std::string signingKey = percentEncode(consumerSecret) + "&" + percentEncode(tokenSecret);

    // Compute HMAC-SHA256
    std::string hmac = hmacSha256(signingKey, baseString);

    // Base64 encode
    return base64Encode(reinterpret_cast<const unsigned char*>(hmac.data()), hmac.size());
}

std::string buildAuthorizationHeader(
    const std::string& httpMethod,
    const std::string& url,
    const std::string& consumerKey,
    const std::string& consumerSecret,
    const std::map<std::string, std::string>& bodyParams,
    int nonceLength)
{
    // Generate OAuth parameters
    std::string timestamp = generateTimestamp();
    std::string nonce = generateNonce(nonceLength);

    // Build combined parameters for signature (OAuth + body params)
    std::map<std::string, std::string> allParams;
    allParams["oauth_consumer_key"] = consumerKey;
    allParams["oauth_signature_method"] = "HMAC-SHA256";
    allParams["oauth_timestamp"] = timestamp;
    allParams["oauth_nonce"] = nonce;
    allParams["oauth_version"] = "1.0";

    // Add body parameters to signature
    for (const auto& [key, value] : bodyParams) {
        allParams[key] = value;
    }

    // Compute signature
    std::string signature = computeSignature(httpMethod, url, allParams, consumerSecret);

    // Build Authorization header (OAuth params only, not body params)
    std::ostringstream header;
    header << "OAuth ";
    header << "oauth_consumer_key=\"" << percentEncode(consumerKey) << "\", ";
    header << "oauth_signature_method=\"HMAC-SHA256\", ";
    header << "oauth_timestamp=\"" << timestamp << "\", ";
    header << "oauth_nonce=\"" << percentEncode(nonce) << "\", ";
    header << "oauth_version=\"1.0\", ";
    header << "oauth_signature=\"" << percentEncode(signature) << "\"";

    return header.str();
}

}  // namespace oauth1
}  // namespace httpcl
