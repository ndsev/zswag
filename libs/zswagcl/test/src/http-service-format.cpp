#include <catch2/catch_all.hpp>

#include "../src/http-service-format.hpp"

using namespace zswagcl;
using Format = zswagcl::HTTPService::Config::Parameter::Format;

TEST_CASE("string formatting", "[zswagcl::http-service-format]") {
    SECTION("Signed integer types") {
        REQUIRE(formatField(Format::String, std::int8_t(-123)) == "-123");
        REQUIRE(formatField(Format::String, std::int16_t(-1234)) == "-1234");
        REQUIRE(formatField(Format::String, std::int32_t(-12345)) == "-12345");
        REQUIRE(formatField(Format::String, std::int64_t(-123456)) == "-123456");
    }
    SECTION("Unsigned integer types") {
        REQUIRE(formatField(Format::String, std::uint8_t(123)) == "123");
        REQUIRE(formatField(Format::String, std::uint16_t(1234)) == "1234");
        REQUIRE(formatField(Format::String, std::uint32_t(12345)) == "12345");
        REQUIRE(formatField(Format::String, std::uint64_t(123456)) == "123456");
    }
    SECTION("String") {
        REQUIRE(formatField(Format::String, std::string("Test String")) == "Test String");
    }
    SECTION("Buffer") {
        REQUIRE(formatField(Format::String, reinterpret_cast<const uint8_t*>("Test Buffer"), 11) == "Test Buffer");
    }
}

TEST_CASE("hex formatting", "[zswagcl::http-service-format]") {
    SECTION("Signed integer types") {
        REQUIRE(formatField(Format::Hex, std::int8_t(-123)) == "-7b");
        REQUIRE(formatField(Format::Hex, std::int16_t(-1234)) == "-4d2");
        REQUIRE(formatField(Format::Hex, std::int32_t(-12345)) == "-3039");
        REQUIRE(formatField(Format::Hex, std::int64_t(-123456)) == "-1e240");
    }
    SECTION("Unsigned integer types") {
        REQUIRE(formatField(Format::Hex, std::uint8_t(123)) == "7b");
        REQUIRE(formatField(Format::Hex, std::uint16_t(1234)) == "4d2");
        REQUIRE(formatField(Format::Hex, std::uint32_t(12345)) == "3039");
        REQUIRE(formatField(Format::Hex, std::uint64_t(123456)) == "1e240");
    }
    SECTION("String") {
        REQUIRE(formatField(Format::Hex, std::string("Test String")) == "5465737420537472696e67");
    }
    SECTION("Buffer") {
        REQUIRE(formatField(Format::Hex, reinterpret_cast<const uint8_t*>("Test Buffer"), 11) == "5465737420427566666572");
    }
}

TEST_CASE("base64 formatting", "[zswagcl::http-service-format]") {
    SECTION("String") {
        REQUIRE(formatField(Format::Base64, std::string("Test String")) == "VGVzdCBTdHJpbmc=");
        REQUIRE(formatField(Format::Base64url, std::string("Test String")) == "VGVzdCBTdHJpbmc");
    }
    SECTION("Buffer") {
        REQUIRE(formatField(Format::Base64, reinterpret_cast<const uint8_t*>("Test Buffer"), 11) == "VGVzdCBCdWZmZXI=");
        REQUIRE(formatField(Format::Base64url, reinterpret_cast<const uint8_t*>("Test Buffer"), 11) == "VGVzdCBCdWZmZXI");
    }
}

TEST_CASE("binary formatting", "[zswagcl::http-service-format]") {
    SECTION("Unsigned integer") {
        REQUIRE(formatField(Format::Binary, std::uint8_t(123)) == "\x7b");
        REQUIRE(formatField(Format::Binary, std::uint16_t(1234)) == "\xd2\x04");
        REQUIRE(formatField(Format::Binary, std::uint32_t(12345)) == std::string("\x39\x30\x00\x00", 4));
        REQUIRE(formatField(Format::Binary, std::uint64_t(123456)) == std::string("\x40\xe2\x01\x00\x00\x00\x00\x00", 8));
    }
    SECTION("String") {
        REQUIRE(formatField(Format::Binary, std::string("Test String")) == "Test String");
    }
    SECTION("Buffer") {
        REQUIRE(formatField(Format::Binary, reinterpret_cast<const uint8_t*>("Test Buffer"), 11) == "Test Buffer");
    }
}
