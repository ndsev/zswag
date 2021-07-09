#include "zswagcl/zsr-client.hpp"
#include "stx/format.h"

#include <iostream>

#include <zsr/types.hpp>
#include <zsr/find.hpp>
#include <zsr/getset.hpp>

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

    auto runTest = [&] (auto const& fn, auto expect, std::string const& aspect)
    {
        ++testCounter;
        std::cout << stx::format("[cpp-test-client] Executing test #{}: {} ...", testCounter, aspect) << std::endl;
        try
        {
            std::cout << "[cpp-test-client]   → Instantiating client." << std::endl;
            auto httpClient = std::make_unique<HttpLibHttpClient>();
            auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
            auto zsrClient = ZsrClient(openApiConfig, std::move(httpClient));
            std::cout << "[cpp-test-client]   → Running request." << std::endl;
            auto responseObject = fn(zsrClient);
            auto response = zsr::get(responseObject, "value").template get<decltype(expect)>().value();
            if (response == expect)
                std::cout << "[cpp-test-client]   → Success." << std::endl;
            else
                throw std::runtime_error(stx::format("Expected {}, got {}!", expect, response));
        }
        catch(std::exception const& e) {
            ++failureCounter;
            std::cout << stx::format("[cpp-test-client]   → ERROR: {}", e.what()) << std::endl;
        }
    };

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.power")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.BaseAndExponent", {
                {"base.value",     2},
                {"exponent.value", 3}
        })).get<zsr::Introspectable>().value();
    }, 8., "Pass fields in path and header");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.intSum")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Integers", {
                {"values", std::vector{100, -200, 400}}
        })).get<zsr::Introspectable>().value();
    }, 300., "Pass hex-encoded array in query");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.byteSum")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Bytes", {
                    {"values", std::vector<uint8_t>{8, 16, 32, 64}}
            })).get<zsr::Introspectable>().value();
    }, 120., "Pass base64url-encoded byte array in path");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.intMul")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Integers", {
                {"values", std::vector{1, 2, 3, 4}}
        })).get<zsr::Introspectable>().value();
    }, 24., "Pass base64-encoded long array in path");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.floatMul")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Doubles", {
                    {"values", std::vector<double>{34.5, 2.}}
            })).get<zsr::Introspectable>().value();
    }, 69., "Pass float array in query.");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.bitMul")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Bools", {
                {"values", std::vector<bool>{true, false}}
            })).get<zsr::Introspectable>().value();
    }, false, "Pass bool array in query (expect false).");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.bitMul")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Bools", {
                    {"values", std::vector<bool>{true, true}}
            })).get<zsr::Introspectable>().value();
    }, true, "Pass bool array in query (expect true).");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.identity")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Double", {
                {"value", 1.}
        })).get<zsr::Introspectable>().value();
    }, 1., "Pass request as blob in body");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.concat")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Strings", {
                {"values", std::vector<std::string>{"foo", "bar"}}
            })).get<zsr::Introspectable>().value();
    }, std::string("foobar"), "Pass base64-encoded strings.");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.name")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.EnumWrapper", {
                {"value", 42}
            })).get<zsr::Introspectable>().value();
    }, std::string("TEST_ENUM_0"), "Pass enum.");

    if (failureCounter > 0) {
        std::cout << stx::format("[cpp-test-client] Done, {} test(s) failed!", failureCounter);
        exit(1);
    }
    std::cout << stx::format("[cpp-test-client] All tests succeeded.", failureCounter);
}
