#include <catch2/catch.hpp>

#include "http-client/uri.hpp"

TEST_CASE("Valid URIs are parsed correctly", "[uri]") {
    SECTION("Empty") {
        REQUIRE_THROWS(
            ndsafw::URIComponents::fromStrRfc3986("")
        );
    }

    SECTION("Scheme+Host") {
        ndsafw::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = ndsafw::URIComponents::fromStrRfc3986("http://host")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "host");
    }

    SECTION("Scheme+IPv4") {
        ndsafw::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = ndsafw::URIComponents::fromStrRfc3986("http://1.1.1.1")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "1.1.1.1");
    }

    SECTION("Scheme+User+Host") {
        ndsafw::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = ndsafw::URIComponents::fromStrRfc3986("http://user:pass@host")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "host");
    }

    SECTION("Scheme+IPv6+Port") {
        ndsafw::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = ndsafw::URIComponents::fromStrRfc3986("http://[::1]:123")
        );

        REQUIRE(uri.scheme == "http");
        REQUIRE(uri.host == "[::1]");
        REQUIRE(uri.port == 123);
    }

    SECTION("Path") {
        ndsafw::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = ndsafw::URIComponents::fromStrRfc3986("http://host/%3c%3E/%20/end")
        );

        REQUIRE(uri.path == "/<>/ /end");
    }

    SECTION("Query") {
        ndsafw::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = ndsafw::URIComponents::fromStrRfc3986("http://host?i(@am=the&query%3e")
        );

        REQUIRE(uri.query == "i(@am=the&query>");
    }

    SECTION("Query+Fragment") {
        ndsafw::URIComponents uri;
        REQUIRE_NOTHROW(
            uri = ndsafw::URIComponents::fromStrRfc3986("http://host?query#fragment")
        );

        REQUIRE(uri.query == "query");
    }
}

TEST_CASE("Build URIs", "[uri-builder]") {
    SECTION("Missing scheme") {
        ndsafw::URIComponents builder;
        builder.host = "host";

        REQUIRE_THROWS(
            builder.build()
        );
    }

    SECTION("Missing host") {
        ndsafw::URIComponents builder;
        builder.scheme = "scheme";

        REQUIRE_THROWS(
            builder.build()
        );
    }

    SECTION("Full URI") {
        ndsafw::URIComponents builder;
        builder.scheme = "ftp";
        builder.host = "host";
        builder.port = 123;
        builder.appendPath("/this/is/:)/the/path");
        builder.query = "hello;";
        builder.addQuery("<var>", "<value>");

        REQUIRE(builder.build() == "ftp://host:123/this/is/%3a%29/the/path?hello%3b&%3cvar%3e=%3cvalue%3e");
    }
}
