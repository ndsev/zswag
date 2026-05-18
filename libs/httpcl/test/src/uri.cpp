#include <catch2/catch_all.hpp>

#include "httpcl/uri.hpp"

TEST_CASE("Valid URIs are parsed correctly", "[uri]") {
    SECTION("Empty") {
        REQUIRE_THROWS(
            httpcl::URIComponents::fromStrRfc3986("")
        );
    }

    SECTION("Scheme+Host") {
        httpcl::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = httpcl::URIComponents::fromStrRfc3986("http://host")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "host");
    }

    SECTION("Scheme+IPv4") {
        httpcl::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = httpcl::URIComponents::fromStrRfc3986("http://1.1.1.1")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "1.1.1.1");
    }

    SECTION("Scheme+User+Host") {
        httpcl::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = httpcl::URIComponents::fromStrRfc3986("http://user:pass@host")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "host");
    }

    SECTION("Scheme+IPv6+Port") {
        httpcl::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = httpcl::URIComponents::fromStrRfc3986("http://[::1]:123")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "[::1]");
        REQUIRE(uri.port == 123);
    }

    SECTION("Path") {
        httpcl::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = httpcl::URIComponents::fromStrRfc3986("http://host/%3c%3E/%20/end")
        );

        REQUIRE(uri.path == "/<>/ /end");
    }

    SECTION("Query") {
        httpcl::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = httpcl::URIComponents::fromStrRfc3986("http://host?i(@am=the&query%3e")
        );

        REQUIRE(uri.query == "i(@am=the&query>");
    }

    SECTION("Query+Fragment") {
        httpcl::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = httpcl::URIComponents::fromStrRfc3986("http://host?query#fragment")
        );

        REQUIRE(uri.query == "query");
    }
}

TEST_CASE("Build URIs", "[uri-builder]") {
    SECTION("Missing scheme") {
        httpcl::URIComponents builder;
        builder.host = "host";

        REQUIRE_THROWS(
            builder.build()
        );
    }

    SECTION("Missing host") {
        httpcl::URIComponents builder;
        builder.scheme = "scheme";

        REQUIRE_THROWS(
            builder.build()
        );
    }

    SECTION("Full URI") {
        httpcl::URIComponents builder;
        builder.scheme = "ftp";
        builder.host = "host";
        builder.port = 123;
        builder.appendPath("/this/is/:)/the/path");
        builder.query = "hello;";
        builder.addQuery("<var>", "<value>");

        REQUIRE(builder.build() == "ftp://host:123/this/is/%3a)/the/path?hello;&%3cvar%3e=%3cvalue%3e");
    }
}

TEST_CASE("Resolve URI reference against base", "[uri-resolve]") {
    using httpcl::URIComponents;

    // Typical OpenAPI scenario: spec at https://api.example.com/v1/openapi.json,
    // various server URL forms.
    auto base = URIComponents::fromStrRfc3986("https://api.example.com/v1/openapi.json");

    SECTION("Absolute reference returned as-is") {
        auto r = URIComponents::resolveReference("https://other.example.com/api", base);
        REQUIRE(r.scheme == "https");
        REQUIRE(r.host == "other.example.com");
        REQUIRE(r.path == "/api");
    }

    SECTION("Server-relative path replaces base path entirely") {
        auto r = URIComponents::resolveReference("/v2", base);
        REQUIRE(r.scheme == "https");
        REQUIRE(r.host == "api.example.com");
        REQUIRE(r.path == "/v2");
    }

    SECTION("Document-relative '.' resolves to base directory") {
        auto r = URIComponents::resolveReference(".", base);
        REQUIRE(r.scheme == "https");
        REQUIRE(r.host == "api.example.com");
        REQUIRE(r.path == "/v1/");
    }

    SECTION("Document-relative './v2' appends to base directory") {
        auto r = URIComponents::resolveReference("./v2", base);
        REQUIRE(r.path == "/v1/v2");
    }

    SECTION("Document-relative bare 'v2' (no './' prefix) treated like './v2'") {
        auto r = URIComponents::resolveReference("v2", base);
        REQUIRE(r.path == "/v1/v2");
    }

    SECTION("Document-relative '../v2' resolves one directory up") {
        auto r = URIComponents::resolveReference("../v2", base);
        REQUIRE(r.path == "/v2");
    }

    SECTION("Document-relative '../../v2' resolves further up; capped at root") {
        // Two levels up from /v1/openapi.json -> /v2 (clamped — can't go above root)
        auto r = URIComponents::resolveReference("../../v2", base);
        REQUIRE(r.path == "/v2");
    }

    SECTION("Empty reference returns base unchanged") {
        auto r = URIComponents::resolveReference("", base);
        REQUIRE(r.scheme == base.scheme);
        REQUIRE(r.host == base.host);
        REQUIRE(r.path == base.path);
    }

    SECTION("Reference with query string preserves query") {
        auto r = URIComponents::resolveReference("./v2?token=abc", base);
        REQUIRE(r.path == "/v1/v2");
        REQUIRE(r.query == "token=abc");
    }

    SECTION("Base port carries over") {
        auto basePort = URIComponents::fromStrRfc3986("http://localhost:8080/api/openapi.json");
        auto r = URIComponents::resolveReference(".", basePort);
        REQUIRE(r.host == "localhost");
        REQUIRE(r.port == 8080);
        REQUIRE(r.path == "/api/");
    }

    SECTION("Relative reference against base lacking scheme throws") {
        URIComponents bareBase;
        bareBase.path = "/foo/bar";
        REQUIRE_THROWS_AS(
            URIComponents::resolveReference(".", bareBase),
            httpcl::URIError);
    }

    SECTION("Resolved URI is buildable") {
        auto r = URIComponents::resolveReference(".", base);
        REQUIRE(r.build() == "https://api.example.com/v1/");
    }
}
