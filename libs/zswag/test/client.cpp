// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.

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
        std::cerr << "The first argument must be the OpenAPI spec URL." << std::endl;
        exit(1);
    }

    auto specUrl = std::string(argv[1]);
    auto testCounter = 0;
    auto failureCounter = 0;

    std::cout << "Starting integration tests with " << specUrl << "\n";

    auto runTest = [&] (
        std::function<zsr::Introspectable(ZsrClient&)> const& fn,
        double expect,
        std::string const& aspect
    ) mutable -> void
    {
        ++testCounter;
        std::cout << stx::format("Executing test #{}: {} ...", testCounter, aspect) << std::endl;
        try
        {
            std::cout << "  ... Instantiating client." << std::endl;
            auto httpClient = std::make_unique<HttpLibHttpClient>();
            auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
            auto zsrClient = ZsrClient(openApiConfig, std::move(httpClient));
            std::cout << "  ... Running request." << std::endl;
            auto responseObject = fn(zsrClient);
            auto response = zsr::get(responseObject, "value").get<double>().value();
            if (response == expect)
                std::cout << "  ... Success." << std::endl;
            else
                // Note: Without the int cast, I get strange errors here on GCC.
                throw std::runtime_error(stx::format("Expected {}, got {}!", (int)expect, (int)response));
        }
        catch(std::exception const& e) {
            ++failureCounter;
            std::cout << stx::format("  ... FAILED: {}", e.what()) << std::endl;
        }
    };

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.power")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.BaseAndExponent", {
                {"base.value",     2},
                {"exponent.value", 3}
        })).get<zsr::Introspectable>().value();
    }, 8., "Pass fields in path and query");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.isum")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Integers", {
                {"values", std::vector{100, -200, 400}}
        })).get<zsr::Introspectable>().value();
    }, 300., "Pass hex-encoded array in query");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.imul")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Integers", {
                {"values", std::vector{1, 2, 3, 4}}
        })).get<zsr::Introspectable>().value();
    }, 24., "Pass base64-encoded long array in path");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.bsum")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Bytes", {
                {"values", std::vector<uint8_t>{8, 16, 32, 64}}
        })).get<zsr::Introspectable>().value();
    }, 120., "Pass base64url-encoded byte array in path");

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.identity")->call(
            zsrClient,
            zsr::make(zsr::packages(), "calculator.Double", {
                {"value", 1.}
        })).get<zsr::Introspectable>().value();
    }, 1., "Pass request as blob in body");

    if (failureCounter > 0) {
        std::cout << stx::format("Done, {} test(s) failed!", failureCounter);
        exit(1);
    }
    std::cout << stx::format("All tests succeeded.", failureCounter);
}
