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
    std::cout << "Starting integration tests with " << specUrl << "\n";

    auto runTest = [&, counter=0] (std::function<zsr::Introspectable(ZsrClient&)> const& fn) mutable {
        // try
        // {
            auto httpClient = std::make_unique<HttpLibHttpClient>();
            auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
            auto zsrClient = ZsrClient(openApiConfig, std::move(httpClient));
            auto responseObject = fn(zsrClient);
            auto response = zsr::get(responseObject, "value").get<double>().value();
            std::cout << stx::format("[test#1 success, got val={}]", response) << std::endl;
        // }
        // catch(std::exception const& e) {
        //     std::cerr << e.what() << std::endl;
        //     std::cout << stx::format("[test#{} FAIL, {}]", counter, e.what()) << std::endl;
        // }
        ++counter;
    };

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.power")->call(
                zsrClient,
                zsr::make(zsr::packages(), "calculator.BaseAndExponent", {
                    {"base.value",     2},
                    {"exponent.value", 3}
            })).get<zsr::Introspectable>().value();
    });

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.isum")->call(
                zsrClient,
                zsr::make(zsr::packages(), "calculator.Integers", {
                    {"values", std::vector{100, -200, 400}}
                })).get<zsr::Introspectable>().value();
    });

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.imul")->call(
                zsrClient,
                zsr::make(zsr::packages(), "calculator.Integers", {
                        {"values", std::vector{1, 2, 3, 4}}
                })).get<zsr::Introspectable>().value();
    });

    runTest([](ZsrClient& zsrClient){
        return zsr::find<zsr::ServiceMethod>("calculator.Calculator.bsum")->call(
                zsrClient,
                zsr::make(zsr::packages(), "calculator.Bytes", {
                        {"values", std::vector<uint8_t>{8, 16, 32, 64}}
                })).get<zsr::Introspectable>().value();
    });
}
