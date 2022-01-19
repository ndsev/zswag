#include "zswagcl/zsr-client.hpp"
#include "stx/format.h"

#include <iostream>

#include "calculator/Calculator.h"

using namespace zswagcl;
using namespace httpcl;

int main (int argc, char* argv[]) {
    if(argc <= 1) {
        std::cerr << "[cpp-test-client] ERROR: The first argument must be the OpenAPI spec URL." << std::endl;
        exit(1);
    }

    auto specUrl = std::string(argv[1]);
    auto testCounter = 0;
    auto failureCounter = 0;

    std::cout << "[cpp-test-client] Starting integration tests with " << specUrl << "\n";

    auto runTest = [&] (auto const& fn, auto expect, std::string const& aspect, std::function<void(httpcl::Config&)> const& authFun)
    {
        ++testCounter;
        std::cout << stx::format("[cpp-test-client] Executing test #{}: {} ...", testCounter, aspect) << std::endl;
        try
        {
            std::cout << "[cpp-test-client]   → Instantiating client." << std::endl;
            auto httpClient = std::make_unique<HttpLibHttpClient>();
            auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
            httpcl::Config authHttpConf;
            authFun(authHttpConf);
            auto zsrClient = ZsrClient(openApiConfig, std::move(httpClient), authHttpConf);
            std::cout << "[cpp-test-client]   → Running request." << std::endl;
            calculator::Calculator::Client calcClient(zsrClient);
            auto response = fn(calcClient);
            if (response.getValue() == expect)
                std::cout << "[cpp-test-client]   → Success." << std::endl;
            else
                throw std::runtime_error(stx::format("Expected {}, got {}!", expect, response.getValue()));
        }
        catch(std::exception const& e) {
            ++failureCounter;
            std::cout << stx::format("[cpp-test-client]   → ERROR: {}", e.what()) << std::endl;
        }
    };

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::BaseAndExponent req(calculator::I32(2), calculator::I32(3), 0, "", .0, std::vector<bool>{});
        return calcClient.powerMethod(req);
    }, 8., "Pass fields in path and header",
    [](httpcl::Config& conf){});

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Integers req(std::vector<int32_t>{100, -200, 400});
        return calcClient.intSumMethod(req);
    }, 300., "Pass hex-encoded array in query",
    [](httpcl::Config& conf){
        conf.headers.insert({"Authorization", "Bearer 123"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Bytes req(std::vector<uint8_t>{8, 16, 32, 64});
        return calcClient.byteSumMethod(req);
    }, 120., "Pass base64url-encoded byte array in path",
    [](httpcl::Config& conf){
        conf.auth = httpcl::Config::BasicAuthentication{
            "u", "pw", ""
        };
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Integers req(std::vector{1, 2, 3, 4});
        return calcClient.intMulMethod(req);
    }, 24., "Pass base64-encoded long array in path",
    [](httpcl::Config& conf){
        conf.query.insert({"api-key", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Doubles req(std::vector<double>{34.5, 2.});
        return calcClient.floatMulMethod(req);
    }, 69., "Pass float array in query.",
    [](httpcl::Config& conf){
        conf.cookies.insert({"api-cookie", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Bools req(std::vector<bool>{true, false});
        return calcClient.bitMulMethod(req);
    }, false, "Pass bool array in query (expect false).",
    [](httpcl::Config& conf){
        conf.apiKey = "42";
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Bools req(std::vector<bool>{true, true});
        return calcClient.bitMulMethod(req);
    }, true, "Pass bool array in query (expect true).",
    [](httpcl::Config& conf){
        conf.headers.insert({"X-Generic-Token", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Double req(1.);
        return calcClient.identityMethod(req);
    }, 1., "Pass request as blob in body",
    [](httpcl::Config& conf){
        conf.cookies.insert({"api-cookie", "42"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::Strings req(std::vector<std::string>{"foo", "bar"});
        return calcClient.concatMethod(req);
    }, std::string("foobar"), "Pass base64-encoded strings.",
    [](httpcl::Config& conf){
        conf.headers.insert({"Authorization", "Bearer 123"});
    });

    runTest([](calculator::Calculator::Client& calcClient){
        calculator::EnumWrapper req(calculator::Enum::TEST_ENUM_0);
        return calcClient.nameMethod(req);
    }, std::string("TEST_ENUM_0"), "Pass enum.",
    [](httpcl::Config& conf){
        conf.apiKey = "42";
    });

    if (failureCounter > 0) {
        std::cout << stx::format("[cpp-test-client] Done, {} test(s) failed!", failureCounter);
        exit(1);
    }
    std::cout << stx::format("[cpp-test-client] All tests succeeded.", failureCounter);
}
