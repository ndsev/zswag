// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.

#include "zswagcl/zsr-client.hpp"

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
    std::cout << "Connecting to " << specUrl << "\n";
    auto httpClient = std::make_unique<HttpLibHttpClient>();
    auto openApiConfig = fetchOpenAPIConfig(specUrl, *httpClient);
    auto httpService = ZsrClient(openApiConfig, std::move(httpClient));

    auto response = zsr::find<zsr::ServiceMethod>("calculator.Calculator.power")->call(
        httpService,
        zsr::make(zsr::packages(), "calculator.BaseAndExponent", {
            {"base.value", 2},
            {"exponent.value", 3}
        })).get<zsr::Introspectable>().value();

    std::cout << zsr::get(response, "value").get<double>().value() << std::endl;
}
