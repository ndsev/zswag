#include <catch2/catch.hpp>

#include <fstream>

#include "zswagcl/zsr-client.hpp"
#include "zswagcl/openapi-parser.hpp"

#include "zsr/find.hpp"
#include "zsr/variant.hpp"
#include "zsr/reflection-main.hpp"
#include "zsr/getset.hpp"

using namespace zswagcl;

static zsr::Service dummyService(nullptr);
static zsr::ServiceMethod dummyServiceMethod(&dummyService);

static auto makeRequest(std::string_view compound, zsr::VariantMap values)
{
    auto request = zsr::make(zsr::packages().front(), compound, std::move(values));

    zserio::BitStreamWriter writer;
    request.meta()->write(request, writer);

    std::size_t size = 0u;
    auto buffer = writer.getWriteBuffer(size);

    return std::make_tuple(request, std::vector<uint8_t>(buffer, buffer + size));
}

/**
 * Load config template file and replace pathes with input string.
 */
static auto makeConfig(std::string paths)
{
    std::string contents;
    std::ifstream file(TESTDATA "/config-template.json");

    while (!file.eof()) {
        std::array<char, 4000> buffer;
        file.read(buffer.data(), buffer.size());

        contents.append(buffer.data(), file.gcount());
    }

    contents = stx::replace_with(contents, "<<PATHS>>", paths);

    std::istringstream ss(contents);
    return parseOpenAPIConfig(ss);
}

TEST_CASE("HTTP-Service", "[zsr-client]") {
    SECTION("fetch server config") {
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

    SECTION("path parameters") {
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
        auto [request, buffer] = makeRequest("Request", {
            {"str", "hello"},
            {"strLen", 3},
            {"strArray", std::vector<std::string>{"a", "b", "c"}}});

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
        auto service = ZsrClient(config, std::move(client));

        zsr::ServiceMethod::Context ctx{dummyService, dummyServiceMethod, request};
        std::vector<uint8_t> response;
        service.callMethod("multi", buffer, response, &ctx);

        /* Check result */
        REQUIRE(getCalled);
    }

    SECTION("query parameters") {
        /* Setup mock client */
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        /* Define mock behavior */
        client->getFun = [&](std::string_view uri) {
            getCalled = true;

            REQUIRE(uri == "https://my.server.com/api/q?"
                           "firstName=Alex&" /* x-form-obj */
                           "form=hello&"
                           "form-arr=a,b,c&"
                           "form-obj=firstName,Alex,role,admin&"
                           "role=admin&" /* x-form-obj */
                           "x-form-arr=a&x-form-arr=b&x-form-arr=c");

            return httpcl::IHttpClient::Result{200, {}};
        };

        /* Create request object */
        auto [request, buffer] = makeRequest("Request", {
            {"str", "hello"},
            {"strLen", 3},
            {"strArray", std::vector<std::string>{"a", "b", "c"}},
            {"flat.role", "admin"},
            {"flat.firstName", "Alex"}});

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
                        }, {
                            "name": "x-form-obj",
                            "in": "query",
                            "style": "form",
                            "explode": True,
                            "x-zserio-request-part": "flat"
                        }, {
                            "name": "form-obj",
                            "in": "query",
                            "style": "form",
                            "explode": False,
                            "x-zserio-request-part": "flat"
                        }
                    ]
                }
            }
        )json");
        auto service = ZsrClient(config, std::move(client));

        zsr::ServiceMethod::Context ctx{dummyService, dummyServiceMethod, request};
        std::vector<uint8_t> response;
        service.callMethod("q", buffer, response, &ctx);

        /* Check result */
        REQUIRE(getCalled);
    }

    SECTION("http post request-buffer") {
        auto postCalled = false;

        /* Make request */
        auto [request, buffer_] = makeRequest("Request", {{"str", "hello"}});
        const auto &buffer = buffer_; /* llvm does not allow capturing bindings in lambdas. */

        /* Setup mock client */
        auto client = std::make_unique<httpcl::MockHttpClient>();
        client->postFun = [&](std::string_view uri,
                              std::string_view body,
                              std::string_view type) {
            REQUIRE(uri == "https://my.server.com/api/post/hello");
            REQUIRE(type == "application/binary");
            REQUIRE(std::equal(body.begin(), body.end(),
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
                    ]
                }
            }
        )json");
        auto service = ZsrClient(config, std::move(client));

        zsr::ServiceMethod::Context ctx{dummyService, dummyServiceMethod, request};
        std::vector<uint8_t> response;
        service.callMethod("post", buffer, response, &ctx);

        /* Check result */
        REQUIRE(postCalled);
    }
}
