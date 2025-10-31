#include <catch2/catch_all.hpp>

#include "httpcl/oauth1-signature.hpp"

#include <regex>
#include <thread>
#include <chrono>

TEST_CASE("OAuth1 nonce generation", "[oauth1][nonce]") {

    SECTION("Generate nonce with default length") {
        std::string nonce = httpcl::oauth1::generateNonce();
        REQUIRE(nonce.length() == 16);
        REQUIRE_FALSE(nonce.empty());

        // Should be alphanumeric
        REQUIRE(std::regex_match(nonce, std::regex("^[A-Za-z0-9]+$")));
    }

    SECTION("Generate nonce with custom length") {
        std::string nonce32 = httpcl::oauth1::generateNonce(32);
        REQUIRE(nonce32.length() == 32);
        REQUIRE(std::regex_match(nonce32, std::regex("^[A-Za-z0-9]+$")));

        std::string nonce8 = httpcl::oauth1::generateNonce(8);
        REQUIRE(nonce8.length() == 8);

        std::string nonce64 = httpcl::oauth1::generateNonce(64);
        REQUIRE(nonce64.length() == 64);
    }

    SECTION("Reject invalid nonce lengths") {
        REQUIRE_THROWS_WITH(
            httpcl::oauth1::generateNonce(7),
            Catch::Matchers::ContainsSubstring("must be between 8 and 64")
        );

        REQUIRE_THROWS_WITH(
            httpcl::oauth1::generateNonce(65),
            Catch::Matchers::ContainsSubstring("must be between 8 and 64")
        );
    }

    SECTION("Generate unique nonces") {
        std::string nonce1 = httpcl::oauth1::generateNonce();
        std::string nonce2 = httpcl::oauth1::generateNonce();
        REQUIRE(nonce1 != nonce2);  // Very unlikely to be equal
    }
}

TEST_CASE("OAuth1 timestamp generation", "[oauth1][timestamp]") {

    SECTION("Generate valid timestamp") {
        std::string timestamp = httpcl::oauth1::generateTimestamp();
        REQUIRE_FALSE(timestamp.empty());

        // Should be numeric
        REQUIRE(std::regex_match(timestamp, std::regex("^[0-9]+$")));

        // Should be approximately current time (2020s = 16xxxxxxxx)
        long long ts = std::stoll(timestamp);
        REQUIRE(ts > 1600000000);  // After ~2020
        REQUIRE(ts < 2000000000);  // Before ~2033
    }

    SECTION("Timestamps increase over time") {
        std::string ts1 = httpcl::oauth1::generateTimestamp();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::string ts2 = httpcl::oauth1::generateTimestamp();

        REQUIRE(std::stoll(ts2) > std::stoll(ts1));
    }
}

TEST_CASE("OAuth1 signature computation", "[oauth1][signature]") {

    SECTION("Compute signature with known test vector") {
        // This is a simplified test vector
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";

        std::map<std::string, std::string> params;
        params["oauth_consumer_key"] = "test-client-id";
        params["oauth_signature_method"] = "HMAC-SHA256";
        params["oauth_timestamp"] = "1234567890";
        params["oauth_nonce"] = "abcdef123456";
        params["oauth_version"] = "1.0";
        params["grant_type"] = "client_credentials";

        std::string consumerSecret = "test-client-secret";

        std::string signature = httpcl::oauth1::computeSignature(
            httpMethod, url, params, consumerSecret);

        // Signature should be base64-encoded (A-Za-z0-9+/=)
        REQUIRE_FALSE(signature.empty());
        REQUIRE(std::regex_match(signature, std::regex("^[A-Za-z0-9+/]+=*$")));
    }

    SECTION("Different parameters produce different signatures") {
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";

        std::map<std::string, std::string> params1;
        params1["oauth_consumer_key"] = "client1";
        params1["oauth_timestamp"] = "1234567890";
        params1["oauth_nonce"] = "nonce1";

        std::map<std::string, std::string> params2;
        params2["oauth_consumer_key"] = "client2";
        params2["oauth_timestamp"] = "1234567890";
        params2["oauth_nonce"] = "nonce1";

        std::string sig1 = httpcl::oauth1::computeSignature(
            httpMethod, url, params1, "secret");
        std::string sig2 = httpcl::oauth1::computeSignature(
            httpMethod, url, params2, "secret");

        REQUIRE(sig1 != sig2);
    }

    SECTION("Different secrets produce different signatures") {
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";

        std::map<std::string, std::string> params;
        params["oauth_consumer_key"] = "client";
        params["oauth_timestamp"] = "1234567890";

        std::string sig1 = httpcl::oauth1::computeSignature(
            httpMethod, url, params, "secret1");
        std::string sig2 = httpcl::oauth1::computeSignature(
            httpMethod, url, params, "secret2");

        REQUIRE(sig1 != sig2);
    }
}

TEST_CASE("OAuth1 Authorization header building", "[oauth1][header]") {

    SECTION("Build header with default nonce length") {
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";
        std::string consumerKey = "test-client-id";
        std::string consumerSecret = "test-secret";

        std::map<std::string, std::string> bodyParams;
        bodyParams["grant_type"] = "client_credentials";

        std::string header = httpcl::oauth1::buildAuthorizationHeader(
            httpMethod, url, consumerKey, consumerSecret, bodyParams);

        // Header should start with "OAuth "
        REQUIRE(header.substr(0, 6) == "OAuth ");

        // Should contain required OAuth parameters
        REQUIRE(header.find("oauth_consumer_key=") != std::string::npos);
        REQUIRE(header.find("oauth_signature_method=\"HMAC-SHA256\"") != std::string::npos);
        REQUIRE(header.find("oauth_timestamp=") != std::string::npos);
        REQUIRE(header.find("oauth_nonce=") != std::string::npos);
        REQUIRE(header.find("oauth_version=\"1.0\"") != std::string::npos);
        REQUIRE(header.find("oauth_signature=") != std::string::npos);

        // Should NOT contain body parameters in header
        REQUIRE(header.find("grant_type") == std::string::npos);
    }

    SECTION("Build header with custom nonce length") {
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";
        std::string consumerKey = "test-client";
        std::string consumerSecret = "test-secret";

        std::string header = httpcl::oauth1::buildAuthorizationHeader(
            httpMethod, url, consumerKey, consumerSecret, {}, 32);

        REQUIRE(header.substr(0, 6) == "OAuth ");
        REQUIRE(header.find("oauth_signature=") != std::string::npos);
    }

    SECTION("Header includes percent-encoded consumer key") {
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";
        std::string consumerKey = "test+client";  // Contains special char
        std::string consumerSecret = "test-secret";

        std::string header = httpcl::oauth1::buildAuthorizationHeader(
            httpMethod, url, consumerKey, consumerSecret);

        // Consumer key should be percent-encoded
        REQUIRE(header.find("oauth_consumer_key=\"test%2Bclient\"") != std::string::npos);
    }

    SECTION("Different calls produce different headers (due to timestamp/nonce)") {
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";
        std::string consumerKey = "client";
        std::string consumerSecret = "secret";

        std::string header1 = httpcl::oauth1::buildAuthorizationHeader(
            httpMethod, url, consumerKey, consumerSecret);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::string header2 = httpcl::oauth1::buildAuthorizationHeader(
            httpMethod, url, consumerKey, consumerSecret);

        REQUIRE(header1 != header2);
    }
}

TEST_CASE("OAuth1 signature with body parameters", "[oauth1][signature][body]") {

    SECTION("Body parameters are included in signature") {
        std::string httpMethod = "POST";
        std::string url = "https://example.com/oauth/token";
        std::string consumerKey = "client";
        std::string consumerSecret = "secret";

        std::map<std::string, std::string> bodyParams1;
        bodyParams1["grant_type"] = "client_credentials";

        std::map<std::string, std::string> bodyParams2;
        bodyParams2["grant_type"] = "refresh_token";

        std::string header1 = httpcl::oauth1::buildAuthorizationHeader(
            httpMethod, url, consumerKey, consumerSecret, bodyParams1);
        std::string header2 = httpcl::oauth1::buildAuthorizationHeader(
            httpMethod, url, consumerKey, consumerSecret, bodyParams2);

        // Extract signatures (they should be different due to different body params)
        auto extractSig = [](const std::string& header) -> std::string {
            size_t pos = header.find("oauth_signature=\"");
            if (pos == std::string::npos) return "";
            pos += 17;  // Length of "oauth_signature=\""
            size_t end = header.find("\"", pos);
            return header.substr(pos, end - pos);
        };

        std::string sig1 = extractSig(header1);
        std::string sig2 = extractSig(header2);

        REQUIRE_FALSE(sig1.empty());
        REQUIRE_FALSE(sig2.empty());
        REQUIRE(sig1 != sig2);
    }
}
