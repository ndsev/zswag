#pragma once

#include <zserio/IService.h>

#include "openapi-client.hpp"
#include "httpcl/http-client.hpp"

namespace zswagcl
{

class ZsrClient : public ::zserio::IServiceClient
{
public:
    ZsrClient(
        zswagcl::OpenAPIConfig config,
        std::unique_ptr<httpcl::IHttpClient> client,
        httpcl::Config httpConfig = {});

    std::vector<uint8_t> callMethod(
        zserio::StringView methodName,
        zserio::RequestData const& requestData,
        void* context) override;

private:
    OpenAPIClient client_;
};

}
