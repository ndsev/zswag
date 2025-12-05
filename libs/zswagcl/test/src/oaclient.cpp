#include <catch2/catch_all.hpp>

#include <fstream>
#include <sstream>

#include "zswagcl/oaclient.hpp"
#include "zswagcl/private/openapi-config.hpp"
#include "zserio/SerializeUtil.h"
#include "service_client_test/Flat.h"
#include "service_client_test/Request.h"
#include "service_client_test/ArrayTestRequest.h"
#include "service_client_test/ComplexArrayTestRequest.h"
#include "service_client_test/ComplexValueTestRequest.h"
#include "service_client_test/Address.h"
#include "service_client_test/Status.h"
#include "service_client_test/Permissions.h"
#include "service_client_test/ExternTestRequest.h"

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

// ============================================================================
// Array and Complex Type Tests
// ============================================================================

TEST_CASE("OAClient - Primitive Arrays", "[oaclient][arrays]") {
    SECTION("Boolean Array Serialization") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // Booleans serialize as 0/1
            REQUIRE(uri.find("bools=1,0,1,1") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ArrayTestRequest request;
        request.setBoolArrayLen(4);
        request.setBoolArray({true, false, true, true});

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testBoolArray",
                    "parameters": [{
                        "name": "bools",
                        "in": "query",
                        "style": "form",
                        "explode": false,
                        "x-zserio-request-part": "boolArray"
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testBoolArray", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Signed Integer Arrays Serialization") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            REQUIRE(uri.find("i8=-128,0,127") != std::string::npos);
            REQUIRE(uri.find("i16=-32768,0,32767") != std::string::npos);
            REQUIRE(uri.find("i32=-100,0,100") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ArrayTestRequest request;
        request.setInt8ArrayLen(3);
        request.setInt8Array({-128, 0, 127});
        request.setInt16ArrayLen(3);
        request.setInt16Array({-32768, 0, 32767});
        request.setInt32ArrayLen(3);
        request.setInt32Array({-100, 0, 100});
        request.setInt64ArrayLen(0);
        request.setInt64Array({});

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testSignedIntArrays",
                    "parameters": [
                        {"name": "i8", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "int8Array"},
                        {"name": "i16", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "int16Array"},
                        {"name": "i32", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "int32Array"}
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testSignedIntArrays", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Unsigned Integer Arrays Serialization") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            REQUIRE(uri.find("u8=0,128,255") != std::string::npos);
            REQUIRE(uri.find("u16=0,32768,65535") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ArrayTestRequest request;
        request.setUint8ArrayLen(3);
        request.setUint8Array({0, 128, 255});
        request.setUint16ArrayLen(3);
        request.setUint16Array({0, 32768, 65535});
        request.setUint32ArrayLen(0);
        request.setUint32Array({});
        request.setUint64ArrayLen(0);
        request.setUint64Array({});

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testUnsignedIntArrays",
                    "parameters": [
                        {"name": "u8", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "uint8Array"},
                        {"name": "u16", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "uint16Array"}
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testUnsignedIntArrays", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Floating Point Arrays Serialization") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // Just verify parameters exist, exact float representation varies
            REQUIRE(uri.find("floats=") != std::string::npos);
            REQUIRE(uri.find("doubles=") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ArrayTestRequest request;
        request.setFloatArrayLen(3);
        request.setFloatArray({0.0f, -1.5f, 3.14f});
        request.setDoubleArrayLen(3);
        request.setDoubleArray({0.0, -1.5, 3.14159});

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testFloatArrays",
                    "parameters": [
                        {"name": "floats", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "floatArray"},
                        {"name": "doubles", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "doubleArray"}
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testFloatArrays", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }

    SECTION("String Array Serialization") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            REQUIRE(uri.find("strings=hello,world,test") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ArrayTestRequest request;
        request.setStringArrayLen(3);
        request.setStringArray({"hello", "world", "test"});

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testStringArray",
                    "parameters": [{
                        "name": "strings",
                        "in": "query",
                        "style": "form",
                        "explode": false,
                        "x-zserio-request-part": "stringArray"
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testStringArray", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Empty Array Handling") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // Empty array should still work
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ArrayTestRequest request;
        request.setStringArrayLen(0);
        request.setStringArray({});

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testEmptyArray",
                    "parameters": [{
                        "name": "empty",
                        "in": "query",
                        "style": "form",
                        "explode": false,
                        "x-zserio-request-part": "stringArray"
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testEmptyArray", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }
}

TEST_CASE("OAClient - Complex Type Arrays", "[oaclient][complex-arrays]") {
    SECTION("Bytes Array Serialization") {
        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            REQUIRE(body);
            REQUIRE(body->contentType == ZSERIO_OBJECT_CONTENT_TYPE);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ComplexArrayTestRequest request;
        request.setBytesArrayLen(2);
        zserio::vector<zserio::vector<uint8_t>> bytesVec;
        bytesVec.push_back({0x01, 0x02, 0x03});
        bytesVec.push_back({0xAA, 0xBB, 0xCC});
        request.setBytesArray(bytesVec);

        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testBytesArray",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testBytesArray", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(postCalled);
    }

    SECTION("Struct Array Serialization") {
        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            REQUIRE(body);
            REQUIRE(body->contentType == ZSERIO_OBJECT_CONTENT_TYPE);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ComplexArrayTestRequest request;
        request.setStructArrayLen(2);
        zserio::vector<service_client_test::Address> addresses;
        addresses.push_back(service_client_test::Address("Main St", 12345));
        addresses.push_back(service_client_test::Address("Oak Ave", 67890));
        request.setStructArray(addresses);

        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testStructArray",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testStructArray", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(postCalled);
    }

    SECTION("Enum and Bitmask Arrays") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // Enums serialize as their underlying values
            REQUIRE(uri.find("enums=0,1,2") != std::string::npos);
            // Bitmasks also serialize as integers
            REQUIRE(uri.find("masks=") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ComplexArrayTestRequest request;
        request.setEnumArrayLen(3);
        zserio::vector<service_client_test::Status> enums;
        enums.push_back(service_client_test::Status::ACTIVE);
        enums.push_back(service_client_test::Status::INACTIVE);
        enums.push_back(service_client_test::Status::PENDING);
        request.setEnumArray(enums);

        request.setBitmaskArrayLen(2);
        zserio::vector<service_client_test::Permissions> masks;
        masks.push_back(service_client_test::Permissions::Values::PERM_READ);
        masks.push_back(service_client_test::Permissions::Values::PERM_WRITE);
        request.setBitmaskArray(masks);

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testEnumBitmaskArrays",
                    "parameters": [
                        {"name": "enums", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "enumArray"},
                        {"name": "masks", "in": "query", "style": "form", "explode": false, "x-zserio-request-part": "bitmaskArray"}
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testEnumBitmaskArrays", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }
}

TEST_CASE("OAClient - Single Complex Types", "[oaclient][complex-types]") {
    SECTION("Single Bytes and Struct") {
        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            REQUIRE(body);
            REQUIRE(body->contentType == ZSERIO_OBJECT_CONTENT_TYPE);
            REQUIRE(body->body.size() > 0);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ComplexValueTestRequest request;
        request.setSingleBytes({0x01, 0x02, 0x03, 0x04});
        request.setSingleStruct(service_client_test::Address("Elm St", 99999));

        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testSingleBytesStruct",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testSingleBytesStruct", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(postCalled);
    }

    SECTION("Single Enum and Bitmask") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // Enum as uint8: 1 for INACTIVE
            REQUIRE(uri.find("status=1") != std::string::npos);
            // Bitmask as uint16
            REQUIRE(uri.find("perms=") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ComplexValueTestRequest request;
        request.setSingleEnum(service_client_test::Status::INACTIVE);
        request.setSingleBitmask(service_client_test::Permissions::Values::PERM_READ |
                                 service_client_test::Permissions::Values::PERM_WRITE);

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testSingleEnumBitmask",
                    "parameters": [
                        {"name": "status", "in": "query", "style": "form", "x-zserio-request-part": "singleEnum"},
                        {"name": "perms", "in": "query", "style": "form", "x-zserio-request-part": "singleBitmask"}
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testSingleEnumBitmask", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(getCalled);
    }
}

TEST_CASE("OAClient - Error Handling", "[oaclient][error]") {
    SECTION("Missing Reflection Data") {
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            FAIL("Should not make HTTP call without reflection");
            return httpcl::IHttpClient::Result{500, {}};
        };

        // Create ServiceData without reflectable
        class NonReflectableServiceData : public zserio::IServiceData {
        public:
            zserio::Span<const uint8_t> getData() const override {
                return zserio::Span<const uint8_t>();
            }
            zserio::IReflectableConstPtr getReflectable() const override {
                return nullptr;
            }
        };

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "test",
                    "parameters": []
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        NonReflectableServiceData badData;

        REQUIRE_THROWS_WITH(
            service.callMethod("test", badData, nullptr),
            Catch::Matchers::ContainsSubstring("Cannot use OAClient")
        );
    }

    SECTION("Missing Field in Request") {
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            FAIL("Should not make HTTP call with missing field");
            return httpcl::IHttpClient::Result{500, {}};
        };

        service_client_test::Request request("test", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testMissingField",
                    "parameters": [{
                        "name": "nonexistent",
                        "in": "query",
                        "x-zserio-request-part": "thisFieldDoesNotExist"
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));

        REQUIRE_THROWS_WITH(
            service.callMethod("testMissingField",
                             zserio::ReflectableServiceData(request.reflectable()),
                             nullptr),
            Catch::Matchers::ContainsSubstring("Could not find field")
        );
    }
}

TEST_CASE("OAClient - Request Extraction Modes", "[oaclient][request-extraction]") {
    SECTION("Whole Request Body Serialization") {
        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        service_client_test::Request request("hello", 2, {"a", "b"},
                                            service_client_test::Flat("admin", "Alice"));
        auto expectedBuffer = zserio::serialize(request);

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            REQUIRE(body);
            REQUIRE(body->contentType == ZSERIO_OBJECT_CONTENT_TYPE);

            // Verify entire request was serialized
            REQUIRE(body->body.size() == expectedBuffer.getByteSize());
            REQUIRE(std::equal(body->body.begin(), body->body.end(),
                             expectedBuffer.getBuffer()));
            return httpcl::IHttpClient::Result{200, {}};
        };

        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testWholeRequest",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testWholeRequest",
                         zserio::ReflectableServiceData(request.reflectable()),
                         nullptr);
        REQUIRE(postCalled);
    }

    SECTION("Individual Field Extraction") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // Verify individual fields extracted correctly
            REQUIRE(uri.find("str=hello") != std::string::npos);
            REQUIRE(uri.find("len=3") != std::string::npos);
            REQUIRE(uri.find("role=admin") != std::string::npos);
            REQUIRE(uri.find("name=Alice") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("hello", 3, {"a", "b", "c"},
                                            service_client_test::Flat("admin", "Alice"));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testFieldExtraction",
                    "parameters": [
                        {"name": "str", "in": "query", "x-zserio-request-part": "str"},
                        {"name": "len", "in": "query", "x-zserio-request-part": "strLen"},
                        {"name": "role", "in": "query", "x-zserio-request-part": "flat.role"},
                        {"name": "name", "in": "query", "x-zserio-request-part": "flat.firstName"}
                    ]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testFieldExtraction",
                         zserio::ReflectableServiceData(request.reflectable()),
                         nullptr);
        REQUIRE(getCalled);
    }
}

TEST_CASE("OAClient - Missing Reflection", "[oaclient][reflection]") {
    SECTION("Call without reflection data throws exception") {
        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testNoReflection",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto client = std::make_unique<httpcl::MockHttpClient>();
        auto service = OAClient(config, std::move(client));

        // Create a reflectable service data without reflection (nullptr)
        zserio::ReflectableServiceData noReflectionData(nullptr);

        REQUIRE_THROWS_AS(
            service.callMethod("testNoReflection", noReflectionData, nullptr),
            std::runtime_error
        );
        REQUIRE_THROWS_WITH(
            service.callMethod("testNoReflection", noReflectionData, nullptr),
            Catch::Matchers::ContainsSubstring("withTypeInfoCode")
        );
    }
}

TEST_CASE("OAClient - Extern/BitBuffer Types", "[oaclient][extern][bitbuffer]") {
    SECTION("Single Extern Field") {
        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            REQUIRE(body);
            REQUIRE(body->contentType == ZSERIO_OBJECT_CONTENT_TYPE);
            // Verify that extern field was serialized
            REQUIRE_FALSE(body->body.empty());
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ExternTestRequest request;

        // Create a BitBuffer for the extern field
        std::vector<uint8_t> externData = {0xDE, 0xAD, 0xBE, 0xEF};
        zserio::BitBuffer singleExternBuffer(externData.data(), externData.size());
        request.setSingleExtern(singleExternBuffer);

        request.setExternArrayLen(0);

        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testSingleExtern",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testSingleExtern", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(postCalled);
    }

    SECTION("Extern Array") {
        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            REQUIRE(body);
            REQUIRE(body->contentType == ZSERIO_OBJECT_CONTENT_TYPE);
            REQUIRE_FALSE(body->body.empty());
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ExternTestRequest request;

        // Create empty single extern
        std::vector<uint8_t> emptyData;
        zserio::BitBuffer emptyBuffer(emptyData.data(), 0);
        request.setSingleExtern(emptyBuffer);

        // Create array of extern fields
        std::vector<zserio::BitBuffer> externArray;

        std::vector<uint8_t> data1 = {0x01, 0x02};
        externArray.emplace_back(data1.data(), data1.size());

        std::vector<uint8_t> data2 = {0x03, 0x04, 0x05};
        externArray.emplace_back(data2.data(), data2.size());

        request.setExternArrayLen(externArray.size());
        request.setExternArray(externArray);

        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testExternArray",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testExternArray", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(postCalled);
    }

    SECTION("Empty Extern Array") {
        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            REQUIRE(body);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::ExternTestRequest request;

        std::vector<uint8_t> data = {0xAB};
        zserio::BitBuffer buffer(data.data(), data.size());
        request.setSingleExtern(buffer);
        request.setExternArrayLen(0);

        auto config = makeConfig(R"json(
            "/test": {
                "post": {
                    "operationId": "testEmptyExternArray",
                    "requestBody": {
                        "content": {
                            "application/x-zserio-object": {
                                "schema": {"type": "string"}
                            }
                        }
                    }
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testEmptyExternArray", zserio::ReflectableServiceData(request.reflectable()), nullptr);
        REQUIRE(postCalled);
    }
}

// ============================================================================
// SecuritySchemeTypeToString and SecuritySchemeMaps Tests
// ============================================================================

TEST_CASE("SecuritySchemeTypeToString", "[oaclient][security]") {
    using SST = OpenAPIConfig::SecuritySchemeType;

    SECTION("All known types have string representations") {
        REQUIRE(std::string(securitySchemeTypeToString(SST::HttpBasic)) == "http/basic");
        REQUIRE(std::string(securitySchemeTypeToString(SST::HttpBearer)) == "http/bearer");
        REQUIRE(std::string(securitySchemeTypeToString(SST::ApiKeyQuery)) == "apiKey/query");
        REQUIRE(std::string(securitySchemeTypeToString(SST::ApiKeyHeader)) == "apiKey/header");
        REQUIRE(std::string(securitySchemeTypeToString(SST::ApiKeyCookie)) == "apiKey/cookie");
        REQUIRE(std::string(securitySchemeTypeToString(SST::OAuth2ClientCredentials)) == "oauth2/clientCredentials");
    }
}

TEST_CASE("SecuritySchemeMaps", "[oaclient][security]") {
    const auto& maps = OpenAPIConfig::SecuritySchemeMaps::instance();

    SECTION("Forward lookup works for all types") {
        REQUIRE(maps.forward.at({"http", "basic"}) == OpenAPIConfig::SecuritySchemeType::HttpBasic);
        REQUIRE(maps.forward.at({"http", "bearer"}) == OpenAPIConfig::SecuritySchemeType::HttpBearer);
        REQUIRE(maps.forward.at({"apiKey", "query"}) == OpenAPIConfig::SecuritySchemeType::ApiKeyQuery);
        REQUIRE(maps.forward.at({"apiKey", "header"}) == OpenAPIConfig::SecuritySchemeType::ApiKeyHeader);
        REQUIRE(maps.forward.at({"apiKey", "cookie"}) == OpenAPIConfig::SecuritySchemeType::ApiKeyCookie);
        REQUIRE(maps.forward.at({"oauth2", "clientCredentials"}) == OpenAPIConfig::SecuritySchemeType::OAuth2ClientCredentials);
    }

    SECTION("Reverse lookup works for all types") {
        using SST = OpenAPIConfig::SecuritySchemeType;
        REQUIRE(maps.reverse.at(SST::HttpBasic) == "http/basic");
        REQUIRE(maps.reverse.at(SST::HttpBearer) == "http/bearer");
        REQUIRE(maps.reverse.at(SST::ApiKeyQuery) == "apiKey/query");
        REQUIRE(maps.reverse.at(SST::ApiKeyHeader) == "apiKey/header");
        REQUIRE(maps.reverse.at(SST::ApiKeyCookie) == "apiKey/cookie");
        REQUIRE(maps.reverse.at(SST::OAuth2ClientCredentials) == "oauth2/clientCredentials");
    }

    SECTION("Invalid forward lookup returns end()") {
        REQUIRE(maps.forward.find({"http", "invalid"}) == maps.forward.end());
        REQUIRE(maps.forward.find({"invalid", "basic"}) == maps.forward.end());
    }
}

TEST_CASE("OpenAPI Security Scheme Parsing Errors", "[oaclient][security][error]") {
    SECTION("Invalid HTTP scheme subtype") {
        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    InvalidHttpScheme:
      type: http
      scheme: digest
paths:
  /test:
    get:
      operationId: test
)";
        std::istringstream ss(yaml);
        REQUIRE_THROWS_WITH(
            parseOpenAPIConfig(ss),
            Catch::Matchers::ContainsSubstring("digest") &&
            Catch::Matchers::ContainsSubstring("basic") &&
            Catch::Matchers::ContainsSubstring("bearer")
        );
    }

    SECTION("Invalid apiKey location") {
        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    InvalidApiKey:
      type: apiKey
      in: body
      name: X-API-Key
paths:
  /test:
    get:
      operationId: test
)";
        std::istringstream ss(yaml);
        REQUIRE_THROWS_WITH(
            parseOpenAPIConfig(ss),
            Catch::Matchers::ContainsSubstring("body") &&
            Catch::Matchers::ContainsSubstring("query") &&
            Catch::Matchers::ContainsSubstring("header") &&
            Catch::Matchers::ContainsSubstring("cookie")
        );
    }

    SECTION("Invalid security scheme type") {
        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    InvalidType:
      type: openIdConnect
paths:
  /test:
    get:
      operationId: test
)";
        std::istringstream ss(yaml);
        REQUIRE_THROWS_WITH(
            parseOpenAPIConfig(ss),
            Catch::Matchers::ContainsSubstring("openIdConnect") &&
            Catch::Matchers::ContainsSubstring("http") &&
            Catch::Matchers::ContainsSubstring("apiKey") &&
            Catch::Matchers::ContainsSubstring("oauth2")
        );
    }

    SECTION("OAuth2 without clientCredentials flow") {
        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    UnsupportedOAuth:
      type: oauth2
      flows:
        authorizationCode:
          authorizationUrl: https://auth.example.com/authorize
          tokenUrl: https://auth.example.com/token
paths:
  /test:
    get:
      operationId: test
)";
        std::istringstream ss(yaml);
        REQUIRE_THROWS_WITH(
            parseOpenAPIConfig(ss),
            Catch::Matchers::ContainsSubstring("clientCredentials")
        );
    }
}

// ============================================================================
// parseParameterSchema Tests - Coverage for openapi-parser.cpp:134-161
// ============================================================================

TEST_CASE("OpenAPI Parameter Schema Format Parsing", "[oaclient][parser][schema]") {

    SECTION("Default format (no schema.format) returns String") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // No encoding transformation for String format
            REQUIRE(uri.find("param=hello") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("hello", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testDefaultFormat",
                    "parameters": [{
                        "name": "param",
                        "in": "query",
                        "x-zserio-request-part": "str"
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testDefaultFormat",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Format 'string' returns String") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            REQUIRE(uri.find("param=hello") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("hello", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testStringFormat",
                    "parameters": [{
                        "name": "param",
                        "in": "query",
                        "x-zserio-request-part": "str",
                        "schema": {
                            "type": "string",
                            "format": "string"
                        }
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testStringFormat",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Format 'byte' returns Base64") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // "hello" in base64 is "aGVsbG8="
            REQUIRE(uri.find("param=aGVsbG8") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("hello", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testByteFormat",
                    "parameters": [{
                        "name": "param",
                        "in": "query",
                        "x-zserio-request-part": "str",
                        "schema": {
                            "type": "string",
                            "format": "byte"
                        }
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testByteFormat",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Format 'base64' returns Base64") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // "hello" in base64 is "aGVsbG8="
            REQUIRE(uri.find("param=aGVsbG8") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("hello", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testBase64Format",
                    "parameters": [{
                        "name": "param",
                        "in": "query",
                        "x-zserio-request-part": "str",
                        "schema": {
                            "type": "string",
                            "format": "base64"
                        }
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testBase64Format",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Format 'base64url' returns Base64url") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // base64url uses URL-safe alphabet (- instead of +, _ instead of /)
            // "hello" in base64url is "aGVsbG8" (no padding by default)
            REQUIRE(uri.find("param=aGVsbG8") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("hello", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testBase64urlFormat",
                    "parameters": [{
                        "name": "param",
                        "in": "query",
                        "x-zserio-request-part": "str",
                        "schema": {
                            "type": "string",
                            "format": "base64url"
                        }
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testBase64urlFormat",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Format 'hex' returns Hex") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // "hello" in hex is "68656c6c6f"
            REQUIRE(uri.find("param=68656c6c6f") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("hello", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testHexFormat",
                    "parameters": [{
                        "name": "param",
                        "in": "query",
                        "x-zserio-request-part": "str",
                        "schema": {
                            "type": "string",
                            "format": "hex"
                        }
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testHexFormat",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("Invalid format throws error with allowed values") {
        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
paths:
  /test:
    get:
      operationId: testInvalidFormat
      parameters:
        - name: param
          in: query
          x-zserio-request-part: str
          schema:
            type: string
            format: invalid_format
)";
        std::istringstream ss(yaml);
        REQUIRE_THROWS_WITH(
            parseOpenAPIConfig(ss),
            Catch::Matchers::ContainsSubstring("invalid_format") &&
            Catch::Matchers::ContainsSubstring("string") &&
            Catch::Matchers::ContainsSubstring("byte") &&
            Catch::Matchers::ContainsSubstring("base64") &&
            Catch::Matchers::ContainsSubstring("hex") &&
            Catch::Matchers::ContainsSubstring("binary")
        );
    }

    SECTION("Header parameter location parsing") {
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            return httpcl::IHttpClient::Result{200, {}};
        };

        service_client_test::Request request("header-value", 0, {},
                                             service_client_test::Flat("", ""));

        auto config = makeConfig(R"json(
            "/test": {
                "get": {
                    "operationId": "testHeaderParam",
                    "parameters": [{
                        "name": "X-Custom-Header",
                        "in": "header",
                        "x-zserio-request-part": "str"
                    }]
                }
            }
        )json");

        auto service = OAClient(config, std::move(client));
        service.callMethod("testHeaderParam",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }
}

// ============================================================================
// Security Handler Error Branch Tests - Coverage for openapi-security.cpp
// ============================================================================

TEST_CASE("Security Handler Error Branches", "[oaclient][security][error-branches]") {

    SECTION("HttpBasicHandler accepts Authorization header without auth config") {
        // This tests lines 27-36 of openapi-security.cpp
        // When ctx.auth is not set but Authorization: Basic header is present

        auto postCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->postFun = [&](std::string_view uri,
                              httpcl::OptionalBodyAndContentType const& body,
                              httpcl::Config const& conf) {
            postCalled = true;
            // Verify the Authorization header is present
            auto authIt = conf.headers.find("Authorization");
            REQUIRE(authIt != conf.headers.end());
            REQUIRE(authIt->second.find("Basic ") == 0);
            return httpcl::IHttpClient::Result{200, {}};
        };

        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    basicAuth:
      type: http
      scheme: basic
security:
  - basicAuth: []
paths:
  /test:
    post:
      operationId: testBasicAuthHeader
      requestBody:
        content:
          application/x-zserio-object:
            schema:
              type: string
)";
        std::istringstream ss(yaml);
        auto config = parseOpenAPIConfig(ss);

        service_client_test::Request request("test", 0, {},
                                             service_client_test::Flat("", ""));

        // Provide Authorization header directly instead of auth config
        httpcl::Config httpConfig;
        httpConfig.headers.insert({"Authorization", "Basic dXNlcjpwYXNz"}); // user:pass

        auto service = OAClient(config, std::move(client), httpConfig);
        service.callMethod("testBasicAuthHeader",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(postCalled);
    }

    SECTION("HttpBearerHandler error message when Authorization header missing") {
        // This tests lines 55-56 of openapi-security.cpp

        auto client = std::make_unique<httpcl::MockHttpClient>();

        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    bearerAuth:
      type: http
      scheme: bearer
paths:
  /test:
    post:
      operationId: testBearerMissing
      security:
        - bearerAuth: []
      requestBody:
        content:
          application/x-zserio-object:
            schema:
              type: string
)";
        std::istringstream ss(yaml);
        auto config = parseOpenAPIConfig(ss);

        service_client_test::Request request("test", 0, {},
                                             service_client_test::Flat("", ""));

        // No Authorization header provided
        httpcl::Config httpConfig;

        auto service = OAClient(config, std::move(client), httpConfig);

        REQUIRE_THROWS_WITH(
            service.callMethod("testBearerMissing",
                              zserio::ReflectableServiceData(request.reflectable()),
                              nullptr),
            Catch::Matchers::ContainsSubstring("Authorization: Bearer")
        );
    }

    SECTION("ApiKeyHandler uses global apiKey fallback for query") {
        // This tests lines 70-72 of openapi-security.cpp

        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            // Verify the API key was added to query
            REQUIRE(uri.find("api_key=my-global-api-key") != std::string::npos);
            return httpcl::IHttpClient::Result{200, {}};
        };

        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    apiKeyAuth:
      type: apiKey
      in: query
      name: api_key
paths:
  /test:
    get:
      operationId: testApiKeyFallback
      security:
        - apiKeyAuth: []
)";
        std::istringstream ss(yaml);
        auto config = parseOpenAPIConfig(ss);

        service_client_test::Request request("test", 0, {},
                                             service_client_test::Flat("", ""));

        // Provide apiKey via global config instead of directly in query
        httpcl::Config httpConfig;
        httpConfig.apiKey = "my-global-api-key";

        auto service = OAClient(config, std::move(client), httpConfig);
        service.callMethod("testApiKeyFallback",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("ApiKeyHandler uses global apiKey fallback for header") {
        // This tests lines 70-72 of openapi-security.cpp for header location

        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            return httpcl::IHttpClient::Result{200, {}};
        };

        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    apiKeyAuth:
      type: apiKey
      in: header
      name: X-API-Key
paths:
  /test:
    get:
      operationId: testApiKeyHeaderFallback
      security:
        - apiKeyAuth: []
)";
        std::istringstream ss(yaml);
        auto config = parseOpenAPIConfig(ss);

        service_client_test::Request request("test", 0, {},
                                             service_client_test::Flat("", ""));

        // Provide apiKey via global config instead of directly in headers
        httpcl::Config httpConfig;
        httpConfig.apiKey = "my-header-api-key";

        auto service = OAClient(config, std::move(client), httpConfig);
        service.callMethod("testApiKeyHeaderFallback",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("ApiKeyHandler uses global apiKey fallback for cookie") {
        // This tests lines 70-72 of openapi-security.cpp for cookie location

        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        client->getFun = [&](std::string_view uri) {
            getCalled = true;
            return httpcl::IHttpClient::Result{200, {}};
        };

        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    apiKeyAuth:
      type: apiKey
      in: cookie
      name: session_token
paths:
  /test:
    get:
      operationId: testApiKeyCookieFallback
      security:
        - apiKeyAuth: []
)";
        std::istringstream ss(yaml);
        auto config = parseOpenAPIConfig(ss);

        service_client_test::Request request("test", 0, {},
                                             service_client_test::Flat("", ""));

        // Provide apiKey via global config instead of directly in cookies
        httpcl::Config httpConfig;
        httpConfig.apiKey = "my-cookie-api-key";

        auto service = OAClient(config, std::move(client), httpConfig);
        service.callMethod("testApiKeyCookieFallback",
                          zserio::ReflectableServiceData(request.reflectable()),
                          nullptr);
        REQUIRE(getCalled);
    }

    SECTION("HttpBasicHandler error message when credentials missing") {
        // This tests line 35-36 of openapi-security.cpp

        auto client = std::make_unique<httpcl::MockHttpClient>();

        std::string yaml = R"(
openapi: "3.0.0"
info:
  title: Test API
  version: "1.0"
servers:
  - url: https://api.example.com
components:
  securitySchemes:
    basicAuth:
      type: http
      scheme: basic
paths:
  /test:
    post:
      operationId: testBasicAuthMissing
      security:
        - basicAuth: []
      requestBody:
        content:
          application/x-zserio-object:
            schema:
              type: string
)";
        std::istringstream ss(yaml);
        auto config = parseOpenAPIConfig(ss);

        service_client_test::Request request("test", 0, {},
                                             service_client_test::Flat("", ""));

        // No auth credentials provided
        httpcl::Config httpConfig;

        auto service = OAClient(config, std::move(client), httpConfig);

        REQUIRE_THROWS_WITH(
            service.callMethod("testBasicAuthMissing",
                              zserio::ReflectableServiceData(request.reflectable()),
                              nullptr),
            Catch::Matchers::ContainsSubstring("basic-auth credentials are missing")
        );
    }
}
