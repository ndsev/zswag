#include <catch2/catch.hpp>

#include <fstream>

#include "zswagcl/http-service.hpp"
#include "zswagcl/openapi-parser.hpp"

#include "zsr/find.hpp"
#include "zsr/variant.hpp"
#include "zsr/reflection-main.hpp"

using namespace zswagcl;

static zsr::Service dummyService(nullptr);
static zsr::ServiceMethod dummyServiceMethod(&dummyService);

static auto make1ParamRequest(int id)
{
    const auto& pkg = zsr::packages().front();
    REQUIRE(pkg.ident == "service_client_test");

    auto reqCompound = zsr::find<zsr::Compound>(pkg, "Request");
    REQUIRE(reqCompound);

    auto reqField = zsr::find<zsr::Field>(*reqCompound, "id");
    REQUIRE(reqField);

    auto request = reqCompound->alloc();
    reqField->set(request, id);

    zserio::BitBuffer buffer(reqCompound->bitSize(request));
    zserio::BitStreamWriter writer(buffer.getBuffer(),
                                   buffer.getByteSize());
    reqCompound->write(request, writer);

    return std::make_tuple(request,
                           std::vector<uint8_t>(buffer.getBuffer(), buffer.getBuffer() + buffer.getByteSize()));
}

static auto make2ParamRequest(std::string first, int second)
{
    const auto& pkg = zsr::packages().front();

    auto multiReqCompound = zsr::find<zsr::Compound>(pkg, "MultiRequest");
    REQUIRE(multiReqCompound);

    auto firstField = zsr::find<zsr::Field>(*multiReqCompound, "id1");
    REQUIRE(firstField);

    auto secondField = zsr::find<zsr::Field>(*multiReqCompound, "id2");
    REQUIRE(secondField);

    auto request = multiReqCompound->alloc();
    firstField->set(request, first);
    secondField->set(request, second);

    zserio::BitBuffer buffer(multiReqCompound->bitSize(request));
    zserio::BitStreamWriter writer(buffer.getBuffer(),
                                   buffer.getByteSize());
    multiReqCompound->write(request, writer);

    return std::make_tuple(request,
                           std::vector<uint8_t>(buffer.getBuffer(), buffer.getBuffer() + buffer.getByteSize()));
}

TEST_CASE("HTTP-Service", "[http-service]") {

    httpcl::MockHttpClient configClient;
    configClient.getFun = [&](const std::string& uri) {
        REQUIRE(uri == "https://dummy");

        std::ifstream spec(TESTDATA "/full-1.json");
        return httpcl::IHttpClient::Result{200, std::string(std::istreambuf_iterator<char>(spec),
                                                            std::istreambuf_iterator<char>())};
    };

    HTTPService::Config config;
    REQUIRE_NOTHROW(
            config = fetchOpenAPIConfig("https://dummy", configClient)
    );


    SECTION("Single URI Parameter") {

        /* Setup mock client */
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        /* Define mock behavior */
        client->getFun = [&](const std::string& uri) {
            getCalled = true;

            REQUIRE(uri == "https://my.server.com/api/single/12345");

            return httpcl::IHttpClient::Result{200, {}};
        };

        /* Create request object */
        auto [request, buffer] = make1ParamRequest(12345);

        /* Fire request */
        auto service = HTTPService(config, std::move(client));

        zsr::ServiceMethod::Context ctx{dummyService, dummyServiceMethod, request};
        std::vector<uint8_t> response;
        service.callMethod("single", buffer, response, &ctx);

        /* Check result */
        REQUIRE(getCalled);
    }

    SECTION("Multiple URI Parameters") {

        /* Setup mock client */
        auto getCalled = false;
        auto client = std::make_unique<httpcl::MockHttpClient>();

        /* Define mock behavior */
        client->getFun = [&](const std::string& uri) {
            getCalled = true;

            REQUIRE(uri == "https://my.server.com/api/multi/hello/12345");

            return httpcl::IHttpClient::Result{200, {}};
        };

        /* Create request object */
        auto [request, buffer] = make2ParamRequest("hello", 12345);

        /* Fire request */
        auto service = HTTPService(config, std::move(client));

        zsr::ServiceMethod::Context ctx{dummyService, dummyServiceMethod, request};
        std::vector<uint8_t> response;
        service.callMethod("multi", buffer, response, &ctx);

        /* Check result */
        REQUIRE(getCalled);

    }
}
