#include <catch2/catch_all.hpp>

#include "httpcl/http-client.hpp"
#include <cstdlib>
#include <sstream>

// Cross-platform environment variable helpers
#ifdef _WIN32
inline void test_setenv(const char* name, const char* value) {
    _putenv_s(name, value);
}
inline void test_unsetenv(const char* name) {
    std::string var = std::string(name) + "=";
    _putenv(var.c_str());
}
#else
inline void test_setenv(const char* name, const char* value) {
    setenv(name, value, 1);
}
inline void test_unsetenv(const char* name) {
    unsetenv(name);
}
#endif

// Helper to capture stderr
class StderrCapture {
public:
    StderrCapture() : old_(std::cerr.rdbuf(buffer_.rdbuf())) {}
    ~StderrCapture() { std::cerr.rdbuf(old_); }
    std::string str() const { return buffer_.str(); }
private:
    std::stringstream buffer_;
    std::streambuf* old_;
};

TEST_CASE("HttpLibHttpClient constructor with environment variables", "[http-client][constructor]") {
    SECTION("HttpLibHttpClient constructor with valid HTTP_TIMEOUT") {
        test_setenv("HTTP_TIMEOUT", "120");
        httpcl::HttpLibHttpClient client;
        test_unsetenv("HTTP_TIMEOUT");

        // The client should be constructed successfully with timeout set to 120
        // We can't directly test the private member, but we verify no exceptions thrown
        REQUIRE_NOTHROW(client.get("http://example.com", httpcl::Config{}));
    }

    SECTION("HttpLibHttpClient constructor with invalid HTTP_TIMEOUT (non-numeric)") {
        test_setenv("HTTP_TIMEOUT", "not-a-number");

        StderrCapture capture;
        httpcl::HttpLibHttpClient client;
        test_unsetenv("HTTP_TIMEOUT");

        // Should print error message to stderr
        REQUIRE(capture.str().find("Could not parse value of HTTP_TIMEOUT") != std::string::npos);
    }

    SECTION("HttpLibHttpClient constructor with empty HTTP_SSL_STRICT") {
        test_setenv("HTTP_SSL_STRICT", "");
        httpcl::HttpLibHttpClient client;
        test_unsetenv("HTTP_SSL_STRICT");

        // When HTTP_SSL_STRICT is empty, sslCertStrict_ should be false
        // The client should construct successfully
        REQUIRE_NOTHROW(client.get("http://example.com", httpcl::Config{}));
    }

    SECTION("HttpLibHttpClient constructor with non-empty HTTP_SSL_STRICT") {
        test_setenv("HTTP_SSL_STRICT", "1");
        httpcl::HttpLibHttpClient client;
        test_unsetenv("HTTP_SSL_STRICT");

        // When HTTP_SSL_STRICT is non-empty, sslCertStrict_ should be true
        // The client should construct successfully
        REQUIRE_NOTHROW(client.get("http://example.com", httpcl::Config{}));
    }
}

TEST_CASE("MockHttpClient untested methods", "[http-client][mock]") {
    httpcl::MockHttpClient client;
    httpcl::Config config;
    httpcl::OptionalBodyAndContentType body = httpcl::BodyAndContentType{
        "test body",
        "application/json"
    };

    SECTION("MockHttpClient put method returns default response") {
        auto result = client.put("http://example.com/resource", body, config);

        REQUIRE(result.status == 0);
        REQUIRE(result.content == "");
    }

    SECTION("MockHttpClient del method returns default response") {
        auto result = client.del("http://example.com/resource", body, config);

        REQUIRE(result.status == 0);
        REQUIRE(result.content == "");
    }

    SECTION("MockHttpClient patch method returns default response") {
        auto result = client.patch("http://example.com/resource", body, config);

        REQUIRE(result.status == 0);
        REQUIRE(result.content == "");
    }
}

TEST_CASE("Query parameter application", "[http-client][query]") {
    httpcl::MockHttpClient client;
    httpcl::Config config;

    SECTION("MockHttpClient get applies query parameters") {
        config.query.insert({"param1", "value1"});
        config.query.insert({"param2", "value2"});

        std::string capturedUri;
        client.getFun = [&capturedUri](std::string_view uri) {
            capturedUri = std::string(uri);
            return httpcl::IHttpClient::Result{200, "OK"};
        };

        auto result = client.get("http://example.com/path", config);

        REQUIRE(result.status == 200);
        REQUIRE(capturedUri.find("param1=value1") != std::string::npos);
        REQUIRE(capturedUri.find("param2=value2") != std::string::npos);
    }

    SECTION("MockHttpClient post applies query parameters") {
        config.query.insert({"qparam", "qvalue"});

        std::string capturedUri;
        client.postFun = [&capturedUri](
            std::string_view uri,
            httpcl::OptionalBodyAndContentType const& body,
            httpcl::Config const& cfg) {
            capturedUri = std::string(uri);
            return httpcl::IHttpClient::Result{201, "Created"};
        };

        httpcl::OptionalBodyAndContentType body = httpcl::BodyAndContentType{
            "post body",
            "text/plain"
        };

        auto result = client.post("http://example.com/create", body, config);

        REQUIRE(result.status == 201);
        REQUIRE(capturedUri.find("qparam=qvalue") != std::string::npos);
    }

    SECTION("Multiple query parameters with special characters") {
        config.query.insert({"key<1>", "value&special"});
        config.query.insert({"key#2", "value=test"});

        std::string capturedUri;
        client.getFun = [&capturedUri](std::string_view uri) {
            capturedUri = std::string(uri);
            return httpcl::IHttpClient::Result{200, "OK"};
        };

        auto result = client.get("http://example.com/endpoint", config);

        REQUIRE(result.status == 200);
        // Verify that special characters are URL encoded
        REQUIRE(capturedUri.find("key%3c1%3e=") != std::string::npos);
        REQUIRE(capturedUri.find("key%232=") != std::string::npos);
    }
}

TEST_CASE("MockHttpClient fallback paths", "[http-client][mock][fallback]") {
    httpcl::MockHttpClient client;
    httpcl::Config config;

    SECTION("MockHttpClient get with no callback set") {
        // getFun is not set, should return default response
        auto result = client.get("http://example.com/test", config);

        REQUIRE(result.status == 0);
        REQUIRE(result.content == "");
    }

    SECTION("MockHttpClient post with no callback set") {
        // postFun is not set, should return default response
        httpcl::OptionalBodyAndContentType body = httpcl::BodyAndContentType{
            "test data",
            "application/json"
        };

        auto result = client.post("http://example.com/submit", body, config);

        REQUIRE(result.status == 0);
        REQUIRE(result.content == "");
    }
}

