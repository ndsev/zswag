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
