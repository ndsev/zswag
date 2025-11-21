#include "zswagcl/oaclient.hpp"
#include "stx/format.h"
#include "spdlog/spdlog.h"

#include "calculator/Calculator.h"

using namespace zswagcl;
using namespace httpcl;

int main (int argc, char* argv[]) {
    if(argc <= 1) {
        spdlog::error("[cpp-test-client] ERROR: The first argument must be the OpenAPI spec URL.");
        exit(1);
    }

    auto specUrl = std::string(argv[1]);
    auto testCounter = 0;
    auto failureCounter = 0;

    spdlog::info("[cpp-test-client] Starting integration tests with {}", specUrl);

    auto runTest = [&] (auto const& fn, auto expect, std::string const& aspect, std::function<void(Config&)> const& authFun)
    {
        ++testCounter;
        spdlog::info("[cpp-test-client] Executing test #{}: {} ...", testCounter, aspect);
        try
        {
            spdlog::info("[cpp-test-client]   => Instantiating client.");
            auto httpClient = std::make_unique<HttpLibHttpClient>();
            auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
            // See https://github.com/spec-first/connexion/issues/1139
            openApiConfig.servers.insert(openApiConfig.servers.begin(), URIComponents::fromStrPath("/bad/path/we/dont/access"));
            Config authHttpConf;
            authFun(authHttpConf);
            auto oaClient = OAClient(openApiConfig, std::move(httpClient), authHttpConf, 1);
            spdlog::info("[cpp-test-client]   => Running request.");
            calculator::Calculator::Client calcClient(oaClient);
            auto response = fn(calcClient);
            if (response.value == expect)
                spdlog::info("[cpp-test-client]   => Success.");
            else
                throw std::runtime_error(stx::format("Expected {}, got {}!", expect, static_cast<decltype(expect)>(response.value)));
        }
        catch(std::exception const& e) {
            ++failureCounter;
            spdlog::error("[cpp-test-client]   => ERROR: {}", e.what());
        }
    };

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::BaseAndExponent req(calculator::I32(2), calculator::I32(3), 0, "", .0, zserio::Vector<zserio::Bool>{});
        return calcClient.powerMethod(req);
    }, 8., "Pass fields in path and header",
    [](Config& conf){});

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Integers req(zserio::Vector<zserio::Int32>{100, -200, 400});
        return calcClient.intSumMethod(req);
    }, 300., "Pass hex-encoded array in query",
    [](Config& conf){
        conf.headers.insert({"Authorization", "Bearer 123"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Bytes req(zserio::Vector<zserio::UInt8>{8, 16, 32, 64});
        return calcClient.byteSumMethod(req);
    }, 120., "Pass base64url-encoded byte array in path",
    [](Config& conf){
        conf.auth = Config::BasicAuthentication{
            "u", "pw", ""
        };
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Integers req(zserio::Vector<zserio::Int32>{1, 2, 3, 4});
        return calcClient.intMulMethod(req);
    }, 24., "Pass base64-encoded long array in path",
    [](Config& conf){
        conf.query.insert({"api-key", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Doubles req(zserio::Vector<zserio::Float64>{34.5, 2.});
        return calcClient.floatMulMethod(req);
    }, 69., "Pass float array in query.",
    [](Config& conf){
        conf.cookies.insert({"api-cookie", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Bools req(zserio::Vector<zserio::Bool>{true, false});
        return calcClient.bitMulMethod(req);
    }, false, "Pass bool array in query (expect false).",
    [](Config& conf){
        conf.apiKey = "42";
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Bools req(zserio::Vector<zserio::Bool>{true, true});
        return calcClient.bitMulMethod(req);
    }, true, "Pass bool array in query (expect true).",
    [](Config& conf){
        conf.headers.insert({"X-Generic-Token", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Double req(1.);
        return calcClient.identityMethod(req);
    }, 1., "Pass request as blob in body",
    [](Config& conf){
        conf.cookies.insert({"api-cookie", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Strings req(zserio::Vector<zserio::String>{"foo", "bar"});
        return calcClient.concatMethod(req);
    }, std::string("foobar"), "Pass base64-encoded strings.",
    [](Config& conf){
        conf.headers.insert({"Authorization", "Bearer 123"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::EnumWrapper req(calculator::Enum::TEST_ENUM_0);
        return calcClient.nameMethod(req);
    }, std::string("TEST_ENUM_0"), "Pass enum.",
    [](Config& conf){
        conf.apiKey = "42";
    });

    if (failureCounter > 0) {
        spdlog::error("[cpp-test-client] Done, {} test(s) failed!", failureCounter);
        exit(1);
    }
    spdlog::info("[cpp-test-client] All tests succeeded.");
}
