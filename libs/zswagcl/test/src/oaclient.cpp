#include <catch2/catch_all.hpp>

#include <fstream>

#include "zswagcl/oaclient.hpp"
#include "zserio/SerializeUtil.h"
#include "service_client_test/Flat.h"
#include "service_client_test/Request.h"

using namespace zswagcl;

/**
 * Load config template file and replace pathes with input string.
 */
static auto makeConfig(std::string pathsReplacement, std::string source = TESTDATA "/config-template.json")
{
    std::string contents;
    std::ifstream file(source);

    while (!file.eof()) {
        std::array<char, 4000> buffer;
        file.read(buffer.data(), buffer.size());

        contents.append(buffer.data(), file.gcount());
    }

    contents = stx::replace_with(contents, "<<PATHS>>", pathsReplacement);

    std::istringstream ss(contents);
    return parseOpenAPIConfig(ss);
}

TEST_CASE("HTTP-Service", "[oaclient]") {
    SECTION("Fetch Server Config") {
        httpcl::MockHttpClient configClient;
        configClient.getFun = [&](std::string_view uri) {
            REQUIRE(uri == "https://dummy");

            std::ifstream file(TESTDATA "/dummy.json");
            return httpcl::IHttpClient::Result{200, std::string(std::istreambuf_iterator<char>(file),
                                                                std::istreambuf_iterator<char>())};
        };

        zswagcl::OpenAPIConfig config;
        REQUIRE_NOTHROW(
            config = fetchOpenAPIConfig("https://dummy", configClient)
        );
    }

    SECTION("Path Parameters") {
        /* Setup mock client */
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        /* Define mock behavior */
        client->getFun = [&](std::string_view uri) {
            getCalled = true;

            REQUIRE(uri == "https://my.server.com/api/multi"
                           "/single/hello/.hello/;matrix=hello"
                           "/array/a,b,c/.a,b,c/;matrix-arr=a;matrix-arr=b;matrix-arr=c");

            return httpcl::IHttpClient::Result{200, {}};
        };

        /* Create request object */
        auto request = service_client_test::Request(
            "hello", 3, std::vector<std::string>{"a", "b", "c"},
            service_client_test::Flat("", ""));

        /* Fire request */
        auto config = makeConfig(R"json(
            "/multi/single/{simple}/{label}/{matrix}/array/{simple-arr}/{label-arr}/{matrix-arr}": {
                "get": {
                    "operationId": "multi",
                    "parameters": [
                        {
                            "name": "simple",
                            "in": "path",
                            "style": "simple",
                            "x-zserio-request-part": "str"
                        }, {
                            "name": "label",
                            "in": "path",
                            "style": "label",
                            "x-zserio-request-part": "str"
                        }, {
                            "name": "matrix",
                            "in": "path",
                            "style": "matrix",
                            "explode": True,
                            "x-zserio-request-part": "str"
                        }, {
                            "name": "simple-arr",
                            "in": "path",
                            "style": "simple",
                            "x-zserio-request-part": "strArray"
                        }, {
                            "name": "label-arr",
                            "in": "path",
                            "style": "label",
                            "x-zserio-request-part": "strArray"
                        }, {
                            "name": "matrix-arr",
                            "in": "path",
                            "style": "matrix",
                            "explode": True,
                            "x-zserio-request-part": "strArray"
                        }
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        auto response = service.callMethod("multi", zserio::ReflectableServiceData(request.reflectable()), nullptr);

        /* Check result */
        REQUIRE(getCalled);
    }

    SECTION("Query Parameters") {
        /* Setup mock client */
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        /* Define mock behavior */
        client->getFun = [&](std::string_view uri) {
            getCalled = true;

            REQUIRE(uri == "https://my.server.com/api/q?"
                           "form=hello&"
                           "form-arr=a,b,c&"
                           "x-form-arr=a&x-form-arr=b&x-form-arr=c");

            return httpcl::IHttpClient::Result{200, {}};
        };

        /* Create request object */
        auto request = service_client_test::Request(
            "hello", 3, std::vector<std::string>{"a", "b", "c"},
            service_client_test::Flat("admin", "Alex"));

        /* Fire request */
        auto config = makeConfig(R"json(
            "/q": {
                "get": {
                    "operationId": "q",
                    "parameters": [
                        {
                            "name": "form",
                            "in": "query",
                            "style": "form",
                            "x-zserio-request-part": "str"
                        }, {
                            "name": "x-form-arr",
                            "in": "query",
                            "style": "form",
                            "explode": True,
                            "x-zserio-request-part": "strArray"
                        }, {
                            "name": "form-arr",
                            "in": "query",
                            "style": "form",
                            "explode": False,
                            "x-zserio-request-part": "strArray"
                        }
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        auto response = service.callMethod("q", zserio::ReflectableServiceData(request.reflectable()), nullptr);

        /* Check result */
        REQUIRE(getCalled);
    }

    SECTION("HTTP Post Request-Buffer") {
        auto postCalled = false;

        /* Create request object */
        auto request = service_client_test::Request(
            "hello", 0, std::vector<std::string>{},
            service_client_test::Flat("", ""));
        auto bitBuf = zserio::serialize(request);
        std::vector<uint8_t> buffer(bitBuf.getBuffer(), bitBuf.getBuffer() + bitBuf.getByteSize());

        /* Setup mock client */
        auto client = std::make_unique<httpcl::MockHttpClient>();
        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf)
        {
            REQUIRE(uri == "https://my.server.com/api/post/hello");
            REQUIRE(body);
            REQUIRE(body->contentType == ZSERIO_OBJECT_CONTENT_TYPE);
            REQUIRE(std::equal(body->body.begin(), body->body.end(),
                               buffer.begin(), buffer.end()));

            postCalled = true;
            return httpcl::IHttpClient::Result{200, {}};
        };

        /* Fire */
        auto config = makeConfig(R"json(
            "/post/{id}": {
                "post": {
                    "operationId": "post",
                    "parameters": [
                        {
                            "name": "id",
                            "in": "path",
                            "x-zserio-request-part": "str"
                        }
                    ],
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": { "type": "string" }
                            }
                        }
                    }
                }
            }
        )json");
        auto service = OAClient(config, std::move(client));
        auto response = service.callMethod("post", zserio::ReflectableServiceData(request.reflectable()), nullptr);

        /* Check result */
        REQUIRE(postCalled);
    }

    SECTION("Authorization Schemes")
    {
        /* Initialize environment */
        static auto httpConfig = std::string("HTTP_SETTINGS_FILE=" TESTDATA "/auth.yaml");

#if _MSC_VER
        _putenv(httpConfig.c_str());
#else
        putenv((char*)httpConfig.c_str());
#endif

        /* Create request object */
        auto request = service_client_test::Request(
            "hello", 0, std::vector<std::string>{},
            service_client_test::Flat("", ""));
        zserio::ReflectableServiceData requestData{request.reflectable()};

        /* Make config, client, service */
        auto config = makeConfig("", TESTDATA "/config-with-auth.json");
        REQUIRE(config.securitySchemes.size() == 5);


        /* Prepare generic test function */
        auto callAndCheck = [&](std::string const& op, std::function<void(httpcl::Config const&)> const& test){
            auto postCalled = false;
            auto client = std::make_unique<httpcl::MockHttpClient>();
            client->postFun = [&](
                std::string_view uri,
                httpcl::OptionalBodyAndContentType const& body,
                httpcl::Config const& conf)
            {
                if (test) test(conf);
                postCalled = true;
                return httpcl::IHttpClient::Result{200, {}};
            };
            auto service = OAClient(config, std::move(client));
            service.callMethod(op, requestData, nullptr);
            REQUIRE(postCalled);
        };

        SECTION("Catch-All Header") {
            REQUIRE_FALSE(config.defaultSecurityScheme.empty());
            REQUIRE_FALSE(config.methodPath["generic"].security);
            REQUIRE_NOTHROW(callAndCheck("generic", [](httpcl::Config const& c){
                REQUIRE(c.headers.find("X-Generic-Token") != c.headers.end());
                REQUIRE(c.headers.find("X-Never-Visible") == c.headers.end());
            }));
        }

        SECTION("API Key") {
            REQUIRE_FALSE(config.methodPath["api-key-auth"].security);
            REQUIRE_NOTHROW(callAndCheck("api-key-auth", [](httpcl::Config const& c){
                REQUIRE(c.headers.find("X-Generic-Token") != c.headers.end());
                REQUIRE(c.headers.find("X-Never-Visible") == c.headers.end());
            }));
        }

        SECTION("Insufficient Credentials") {
            REQUIRE(config.methodPath["bad-auth"].security);
            REQUIRE_THROWS_MATCHES(
                callAndCheck("bad-auth", [](httpcl::Config const& c){}),
                std::runtime_error,
                Catch::Matchers::Predicate<std::runtime_error>(
                    [](std::runtime_error const& e) -> bool {
                        std::string req = "The provided HTTP configuration does not satisfy authentication requirements";
                        std::string msg = e.what();
                        return msg.rfind(req, 0) == 0;
                    }
                )
            );
        }

        SECTION("Combined Cookie and Basic-Auth") {
            REQUIRE(config.methodPath["cookie-and-basic-auth"].security);
            REQUIRE_NOTHROW(callAndCheck("cookie-and-basic-auth", [](httpcl::Config const& c){
                REQUIRE(c.cookies.find("api-cookie") != c.cookies.end());
                REQUIRE(c.auth);
            }));
        }

        SECTION("Bearer-Auth") {
            REQUIRE(config.methodPath["bearer-auth"].security);
            REQUIRE_NOTHROW(callAndCheck("bearer-auth", [](httpcl::Config const& c){
                auto auth = c.headers.find("Authorization");
                REQUIRE(auth != c.headers.end());
                REQUIRE(auth->second == "Bearer 0000");
            }));
        }

        SECTION("Basic-Auth") {
            REQUIRE(config.methodPath["basic-auth"].security);
            REQUIRE_NOTHROW(callAndCheck("basic-auth", [](httpcl::Config const& c){
                REQUIRE(c.auth);
            }));
        }

        SECTION("Query-Token Auth") {
            REQUIRE(config.methodPath["query-auth"].security);
            REQUIRE_NOTHROW(callAndCheck("query-auth", [](httpcl::Config const& c){
                REQUIRE(c.query.find("api-key") != c.query.end());
            }));
        }
    }
}
