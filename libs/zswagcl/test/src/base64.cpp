#include <catch2/catch_all.hpp>

#include "../../src/base64.hpp"

TEST_CASE("Base64 encode", "[base64]") {
    SECTION("Base64") {
        auto res = zswagcl::base64_encode(
            (unsigned char*)"\xC3\x9f\xC3\x9f\xC3\x9f", 6u);

        REQUIRE(res == "w5/Dn8Of");
    }

    SECTION("Base64url") {
        auto res = zswagcl::base64url_encode(
            (unsigned char*)"\xC3\x9f\xC3\x9f\xC3\x9f", 6u);

        REQUIRE(res == "w5_Dn8Of");
    }
}

TEST_CASE("Base64 decode", "[base64]") {
    SECTION("Base64") {
        auto res = zswagcl::base64_decode("w5/Dn8Of");

        REQUIRE(res == "\xC3\x9f\xC3\x9f\xC3\x9f");
    }

    SECTION("Base64url") {
        auto res = zswagcl::base64url_decode("w5_Dn8Of");

        REQUIRE(res == "\xC3\x9f\xC3\x9f\xC3\x9f");
    }
}
