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
