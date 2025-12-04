#include <catch2/catch_all.hpp>

#include "zswagcl/private/openapi-parameter-helper.hpp"

using namespace zswagcl;

using Parameter = OpenAPIConfig::Parameter;
using Format = Parameter::Format;
using PStyle = Parameter::Style;

static auto makeParameter(std::string ident,
                         PStyle style,
                          bool explode,
                          Format format = Format::String)
{
    OpenAPIConfig::Parameter parameter;
    parameter.ident = ident;
    parameter.style = style;
    parameter.explode = explode;
    parameter.format = format;

    return parameter;
}

template <class _Fun>
static auto pathStr(const Parameter& parameter, _Fun fun)
{
    ParameterValueHelper helper(parameter);
    return fun(helper).pathStr(parameter);
}

template <class _Fun>
static auto queryOrHeaderPairs(const Parameter& parameter, _Fun fun)
{
    ParameterValueHelper helper(parameter);
    return fun(helper).queryOrHeaderPairs(parameter);
}

/* Testdata */
const auto value = 5;
const auto list = std::vector<int>{3, 4, 5};
const auto object = std::map<std::string, std::string>{
    {"role", "admin"},
    {"firstName", "Alex"}
};

TEST_CASE("openapi path parameters", "[zswagcl::open-api-format-helper]") {
    SECTION("Style Simple") {
        auto style =PStyle::Simple;

        SECTION("Primitive Value") {
            auto explode = GENERATE(false, true);
            INFO("Explode: " << explode);

            auto r = pathStr(makeParameter("id", style, explode), [&](auto& helper) {
                return helper.value(value);
            });

            REQUIRE(r == "5");
        }

        SECTION("Array") {
            auto explode = GENERATE(false, true);
            INFO("Explode: " << explode);

            auto r = pathStr(makeParameter("id", style, explode), [&](auto& helper) {
                return helper.array(list);
            });

            REQUIRE(r == "3,4,5");
        }

        SECTION("Object") {
            SECTION("Normal") {
                auto r = pathStr(makeParameter("id", style, false), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r == "firstName,Alex,role,admin");
            }
            SECTION("Explode") {
                auto r = pathStr(makeParameter("id", style, true), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r == "firstName=Alex,role=admin");
            }
        }
    }

    SECTION("Style Label") {
        auto style =PStyle::Label;

        SECTION("Primitive Value") {
            auto explode = GENERATE(false, true);
            INFO("Explode: " << explode);

            auto r = pathStr(makeParameter("id", style, explode), [&](auto& helper) {
                return helper.value(value);
            });

            REQUIRE(r == ".5");
        }

        SECTION("Array") {
            auto explode = GENERATE(false, true);
            INFO("Explode: " << explode);

            auto r = pathStr(makeParameter("id", style, explode), [&](auto& helper) {
                return helper.array(list);
            });

            if (explode)
                REQUIRE(r == ".3.4.5");
            else
                REQUIRE(r == ".3,4,5");
        }

        SECTION("Object") {
            SECTION("Normal") {
                auto r = pathStr(makeParameter("id", style, false), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r == ".firstName,Alex,role,admin");
            }
            SECTION("Explode") {
                auto r = pathStr(makeParameter("id", style, true), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r == ".firstName=Alex.role=admin");
            }
        }
    }

    SECTION("Style Matrix") {
        auto style =PStyle::Matrix;

        SECTION("Primitive Value") {
            auto explode = GENERATE(false, true);
            INFO("Explode: " << explode);

            auto r = pathStr(makeParameter("id", style, explode), [&](auto& helper) {
                return helper.value(value);
            });

            REQUIRE(r == ";id=5");
        }

        SECTION("Array") {
            SECTION("Normal") {
                auto r = pathStr(makeParameter("id", style, false), [&](auto& helper) {
                    return helper.array(list);
                });

                REQUIRE(r == ";id=3,4,5");
            }
            SECTION("Explode") {
                auto r = pathStr(makeParameter("id", style, true), [&](auto& helper) {
                    return helper.array(list);
                });

                REQUIRE(r == ";id=3;id=4;id=5");
            }
        }

        SECTION("Object") {
            SECTION("Normal") {
                auto r = pathStr(makeParameter("id", style, false), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r == ";id=firstName,Alex,role,admin");
            }
            SECTION("Explode") {
                auto r = pathStr(makeParameter("id", style, true), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r == ";firstName=Alex;role=admin");
            }
        }
    }
}

TEST_CASE("openapi path parameters - formats", "[zswagcl::open-api-format-helper]") {
    SECTION("Style Simple") {
        auto style =PStyle::Simple;

        SECTION("Format Hex") {
            auto format = Format::Hex;

            SECTION("Binary Value") {
                auto r = pathStr(makeParameter("id", style, false, format), [&](auto& helper) {
                    return helper.binary(std::vector<uint8_t>{ 0xde, 0xad, 0xbe, 0xef });
                });

                REQUIRE(r == "deadbeef");
            }

            SECTION("Primitive Value") {
                auto r = pathStr(makeParameter("id", style, false, format), [&](auto& helper) {
                    return helper.value(255); /* Choose some value that outputs digits > 9. */
                });

                REQUIRE(r == "ff");
            }

            SECTION("Array") {
                auto r = pathStr(makeParameter("id", style, false, format), [&](auto& helper) {
                    return helper.array(std::vector<int>{100, 200, 300});
                });

                REQUIRE(r == "64,c8,12c");
            }

            SECTION("Object") {
                auto r = pathStr(makeParameter("id", style, false, format), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r == "firstName,416c6578,role,61646d696e");
            }
        }

        SECTION("Format Base64URL") {
            auto format = Format::Base64url;

            SECTION("Primitive Value (String)") {
                auto r = pathStr(makeParameter("id", style, false, format), [&](auto& helper) {
                    return helper.value("Hello World!");
                });

                REQUIRE(r == "SGVsbG8gV29ybGQh");
            }

            SECTION("Primitive Value (Integer)") {
                auto r = pathStr(makeParameter("id", style, false, format), [&](auto& helper) {
                    return helper.value(5);
                });

                INFO("Hex: " << stx::to_hex(r.begin(), r.end()));
                REQUIRE(r == "AAAABQ==");
            }
        }

        SECTION("Format Binary") {
            auto format = Format::Binary;

            SECTION("Primitive Value") {
                auto r = pathStr(makeParameter("id", style, false, format), [&](auto& helper) {
                    return helper.value(5);
                });

                INFO("Hex: " << stx::to_hex(r.begin(), r.end()));
                REQUIRE(r == std::string("\x0\x0\x0\x5", 4));
            }
        }
    }
}

TEST_CASE("openapi query parameters", "[zswagcl::open-api-format-helper]") {
    SECTION("Style Form") {
        auto style =PStyle::Form;

        SECTION("Primitive Value") {
            auto explode = GENERATE(false, true);
            INFO("Explode: " << explode);

            auto r = queryOrHeaderPairs(makeParameter("id", style, explode), [&](auto& helper) {
                return helper.value(value);
            });

            REQUIRE(r.size() == 1);

            const auto& [k, v] = r[0];
            REQUIRE(k == "id");
            REQUIRE(v == "5");
        }

        SECTION("Array") {
            SECTION("Normal") {
                auto r = queryOrHeaderPairs(makeParameter("id", style, false), [&](auto& helper) {
                    return helper.array(list);
                });

                REQUIRE(r.size() == 1);

                const auto& [k, v] = r[0];
                REQUIRE(k == "id");
                REQUIRE(v == "3,4,5");
            }
            SECTION("Explode") {
                auto r = queryOrHeaderPairs(makeParameter("id", style, true), [&](auto& helper) {
                    return helper.array(list);
                });

                REQUIRE(r.size() == 3);

                int i = 3;
                for (const auto& [k, v] : r) {
                    REQUIRE(k == "id");
                    REQUIRE(v == stx::to_string(i++));
                }
            }
        }

        SECTION("Object") {
            SECTION("Normal") {
                auto r = queryOrHeaderPairs(makeParameter("id", style, false), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r.size() == 1);

                const auto& [k, v] = r[0];
                REQUIRE(k == "id");
                REQUIRE(v == "firstName,Alex,role,admin");
            }
            SECTION("Explode") {
                auto r = queryOrHeaderPairs(makeParameter("id", style, true), [&](auto& helper) {
                    return helper.object(object);
                });

                REQUIRE(r.size() == 2);
                {
                    const auto& [k, v] = r[0];
                    REQUIRE(k == "firstName");
                    REQUIRE(v == "Alex");
                }
                {
                    const auto& [k, v] = r[1];
                    REQUIRE(k == "role");
                    REQUIRE(v == "admin");
                }
            }
        }

    }
}

TEST_CASE("openapi parameter bodyStr", "[zswagcl::open-api-format-helper]") {
    SECTION("String value returns string") {
        auto param = makeParameter("body", PStyle::Form, false);
        ParameterValueHelper helper(param);

        auto paramValue = helper.value("test content");
        REQUIRE(paramValue.bodyStr() == "test content");
    }

    SECTION("Vector value throws exception") {
        auto param = makeParameter("body", PStyle::Form, false);
        ParameterValueHelper helper(param);

        auto paramValue = helper.array(std::vector<int>{1, 2, 3});
        REQUIRE_THROWS_AS(paramValue.bodyStr(), std::runtime_error);
        REQUIRE_THROWS_WITH(paramValue.bodyStr(), "Expected parameter-value of type string, got vector");
    }

    SECTION("Map value throws exception") {
        auto param = makeParameter("body", PStyle::Form, false);
        ParameterValueHelper helper(param);

        auto paramValue = helper.object(object);
        REQUIRE_THROWS_AS(paramValue.bodyStr(), std::runtime_error);
        REQUIRE_THROWS_WITH(paramValue.bodyStr(), "Expected parameter-value of type string, got dictionary");
    }

    SECTION("Binary value returns string") {
        auto param = makeParameter("body", PStyle::Form, false, Format::Binary);
        ParameterValueHelper helper(param);

        auto paramValue = helper.binary(std::vector<uint8_t>{0x48, 0x65, 0x6C, 0x6C, 0x6F}); // "Hello"
        REQUIRE(paramValue.bodyStr() == "Hello");
    }
}

TEST_CASE("openapi parameter helper - negative integers", "[zswagcl::open-api-format-helper]") {
    SECTION("Negative integer with Hex format") {
        auto param = makeParameter("id", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        auto r = helper.value(-42).pathStr(param);
        REQUIRE(r == "-2a");
    }

    SECTION("Negative integer in array with Hex format") {
        auto param = makeParameter("id", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        auto r = helper.array(std::vector<int>{-10, -20, -30}).pathStr(param);
        REQUIRE(r == "-a,-14,-1e");
    }
}

TEST_CASE("openapi parameter helper - floating point", "[zswagcl::open-api-format-helper]") {
    SECTION("Float with String format") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::String);
        ParameterValueHelper helper(param);

        auto r = helper.value(3.14f).pathStr(param);
        REQUIRE(r == "3.140000");
    }

    SECTION("Double with String format") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::String);
        ParameterValueHelper helper(param);

        auto r = helper.value(2.71828).pathStr(param);
        REQUIRE(r.substr(0, 7) == "2.71828");
    }

    SECTION("Float array with String format") {
        auto param = makeParameter("values", PStyle::Simple, false, Format::String);
        ParameterValueHelper helper(param);

        auto r = helper.array(std::vector<double>{1.1, 2.2, 3.3}).pathStr(param);
        REQUIRE(r == "1.100000,2.200000,3.300000");
    }

    SECTION("Float with Binary format") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::Binary);
        ParameterValueHelper helper(param);

        auto r = helper.value(1.0).pathStr(param);
        REQUIRE(r.size() == sizeof(double));
    }
}

TEST_CASE("openapi parameter helper - Base64 encoding", "[zswagcl::open-api-format-helper]") {
    SECTION("Binary with Base64 format") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Base64);
        ParameterValueHelper helper(param);

        auto r = helper.binary(std::vector<uint8_t>{0x48, 0x65, 0x6C, 0x6C, 0x6F}).pathStr(param);
        REQUIRE(r == "SGVsbG8=");
    }

    SECTION("String with Base64 format") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Base64);
        ParameterValueHelper helper(param);

        auto r = helper.value("Test").pathStr(param);
        REQUIRE(r == "VGVzdA==");
    }

    SECTION("Integer with Base64 format") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Base64);
        ParameterValueHelper helper(param);

        auto r = helper.value(42).pathStr(param);
        REQUIRE_FALSE(r.empty());
    }
}

TEST_CASE("openapi parameter helper - zserio::Span binary", "[zswagcl::open-api-format-helper]") {
    SECTION("Binary from zserio::Span") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        std::vector<uint8_t> data{0xAB, 0xCD, 0xEF};
        zserio::Span<const uint8_t> span(data.data(), data.size());

        auto r = helper.binary(span).pathStr(param);
        REQUIRE(r == "abcdef");
    }

    SECTION("Binary from zserio::Span with Base64") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Base64);
        ParameterValueHelper helper(param);

        std::vector<uint8_t> data{0x01, 0x02, 0x03};
        zserio::Span<const uint8_t> span(data.data(), data.size());

        auto r = helper.binary(span).pathStr(param);
        REQUIRE(r == "AQID");
    }
}

TEST_CASE("openapi parameter helper - Any variant", "[zswagcl::open-api-format-helper]") {
    SECTION("Any with int64_t") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::String);
        Any anyValue = std::int64_t(12345);

        auto result = impl::FormatHelper<Any>::format(Format::String, anyValue);
        REQUIRE(result == "12345");
    }

    SECTION("Any with uint64_t") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::String);
        Any anyValue = std::uint64_t(67890);

        auto result = impl::FormatHelper<Any>::format(Format::String, anyValue);
        REQUIRE(result == "67890");
    }

    SECTION("Any with double") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::String);
        Any anyValue = 3.14159;

        auto result = impl::FormatHelper<Any>::format(Format::String, anyValue);
        REQUIRE(result.substr(0, 6) == "3.1415");
    }

    SECTION("Any with string") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::String);
        Any anyValue = std::string("hello");

        auto result = impl::FormatHelper<Any>::format(Format::String, anyValue);
        REQUIRE(result == "hello");
    }

    SECTION("Any with hex format") {
        Any anyValue = std::int64_t(255);

        auto result = impl::FormatHelper<Any>::format(Format::Hex, anyValue);
        REQUIRE(result == "ff");
    }
}

TEST_CASE("openapi parameter helper - const char*", "[zswagcl::open-api-format-helper]") {
    SECTION("const char* with String format") {
        auto result = impl::FormatHelper<const char*>::format(Format::String, "test");
        REQUIRE(result == "test");
    }

    SECTION("const char* with Binary format") {
        auto result = impl::FormatHelper<const char*>::format(Format::Binary, "data");
        REQUIRE(result == "data");
    }

    SECTION("const char* with Hex format") {
        auto result = impl::FormatHelper<const char*>::format(Format::Hex, "AB");
        REQUIRE(result == "4142");
    }
}

TEST_CASE("openapi parameter helper - edge cases", "[zswagcl::open-api-format-helper]") {
    SECTION("Empty string") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::String);
        ParameterValueHelper helper(param);

        auto r = helper.value("").pathStr(param);
        REQUIRE(r == "");
    }

    SECTION("Empty binary") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        auto r = helper.binary(std::vector<uint8_t>{}).pathStr(param);
        REQUIRE(r == "");
    }

    SECTION("Zero value with Hex") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        auto r = helper.value(0).pathStr(param);
        REQUIRE(r == "0");
    }

    SECTION("Large unsigned value with Hex") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        auto r = helper.value(std::uint64_t(0xDEADBEEFCAFEBABE)).pathStr(param);
        REQUIRE(r == "deadbeefcafebabe");
    }
}

TEST_CASE("openapi parameter helper - htobe and binary formats", "[zswagcl::open-api-format-helper]") {
    SECTION("Integer with Binary format triggers htobe") {
        auto param = makeParameter("value", PStyle::Simple, false, Format::Binary);
        ParameterValueHelper helper(param);

        auto r = helper.value(0x12345678).pathStr(param);
        REQUIRE(r.size() == sizeof(int));
        // Verify it's in big-endian format
    }

    SECTION("int8 with Binary format") {
        auto r = impl::FormatHelper<int8_t>::format(Format::Binary, int8_t(42));
        REQUIRE(r.size() == 1);
        REQUIRE(r[0] == 42);
    }

    SECTION("int16 with Binary format") {
        auto r = impl::FormatHelper<int16_t>::format(Format::Binary, int16_t(0x1234));
        REQUIRE(r.size() == 2);
    }

    SECTION("int32 with Binary format") {
        auto r = impl::FormatHelper<int32_t>::format(Format::Binary, int32_t(0x12345678));
        REQUIRE(r.size() == 4);
    }

    SECTION("int64 with Binary format") {
        auto r = impl::FormatHelper<int64_t>::format(Format::Binary, int64_t(0x123456789ABCDEF0));
        REQUIRE(r.size() == 8);
    }

    SECTION("uint8 with Binary format") {
        auto r = impl::FormatHelper<uint8_t>::format(Format::Binary, uint8_t(255));
        REQUIRE(r.size() == 1);
    }

    SECTION("uint16 with Binary format") {
        auto r = impl::FormatHelper<uint16_t>::format(Format::Binary, uint16_t(0xABCD));
        REQUIRE(r.size() == 2);
    }

    SECTION("uint32 with Binary format") {
        auto r = impl::FormatHelper<uint32_t>::format(Format::Binary, uint32_t(0xDEADBEEF));
        REQUIRE(r.size() == 4);
    }

    SECTION("uint64 with Binary format") {
        auto r = impl::FormatHelper<uint64_t>::format(Format::Binary, uint64_t(0xCAFEBABEDEADBEEF));
        REQUIRE(r.size() == 8);
    }
}

TEST_CASE("openapi parameter helper - vector<uint8_t> formatting", "[zswagcl::open-api-format-helper]") {
    SECTION("vector<uint8_t> with Hex format") {
        std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
        auto r = impl::FormatHelper<std::vector<uint8_t>>::format(Format::Hex, data);
        REQUIRE(r == "deadbeef");
    }

    SECTION("vector<uint8_t> with Base64 format") {
        std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
        auto r = impl::FormatHelper<std::vector<uint8_t>>::format(Format::Base64, data);
        REQUIRE(r == "SGVsbG8=");
    }

    SECTION("vector<uint8_t> with Base64url format") {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto r = impl::FormatHelper<std::vector<uint8_t>>::format(Format::Base64url, data);
        REQUIRE(r == "AQID");
    }

    SECTION("vector<uint8_t> with String format") {
        std::vector<uint8_t> data = {0x41, 0x42, 0x43}; // "ABC"
        auto r = impl::FormatHelper<std::vector<uint8_t>>::format(Format::String, data);
        REQUIRE(r == "ABC");
    }

    SECTION("vector<uint8_t> with Binary format") {
        std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
        auto r = impl::FormatHelper<std::vector<uint8_t>>::format(Format::Binary, data);
        REQUIRE(r.size() == 3);
    }
}

TEST_CASE("openapi parameter helper - floating point binary format", "[zswagcl::open-api-format-helper]") {
    SECTION("float with Binary format") {
        auto r = impl::FormatHelper<float>::format(Format::Binary, 3.14f);
        REQUIRE(r.size() == sizeof(float));
    }

    SECTION("double with Binary format") {
        auto r = impl::FormatHelper<double>::format(Format::Binary, 2.71828);
        REQUIRE(r.size() == sizeof(double));
    }

    SECTION("float with Base64 format") {
        auto r = impl::FormatHelper<float>::format(Format::Base64, 1.0f);
        REQUIRE_FALSE(r.empty());
    }

    SECTION("double with Base64url format") {
        auto r = impl::FormatHelper<double>::format(Format::Base64url, 1.0);
        REQUIRE_FALSE(r.empty());
    }
}

TEST_CASE("openapi parameter helper - binary method overloads", "[zswagcl::open-api-format-helper]") {
    SECTION("binary with rvalue vector") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        auto r = helper.binary(std::vector<uint8_t>{0xCA, 0xFE}).pathStr(param);
        REQUIRE(r == "cafe");
    }

    SECTION("binary with const vector reference") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Hex);
        ParameterValueHelper helper(param);

        const std::vector<uint8_t> data = {0xBE, 0xEF};
        auto r = helper.binary(data).pathStr(param);
        REQUIRE(r == "beef");
    }

    SECTION("binary with zserio::Span") {
        auto param = makeParameter("data", PStyle::Simple, false, Format::Base64url);
        ParameterValueHelper helper(param);

        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        zserio::Span<const uint8_t> span(data.data(), data.size());

        auto r = helper.binary(span).pathStr(param);
        REQUIRE(r == "AQIDBA==");
    }
}
