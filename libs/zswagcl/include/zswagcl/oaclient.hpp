#pragma once

#include <zserio/IService.h>

#include "private/openapi-client.hpp"
#include "httpcl/http-client.hpp"

namespace zswagcl
{

class OAClient : public ::zserio::IServiceClient
{
public:
    OAClient(
        OpenAPIConfig config,
        std::unique_ptr<httpcl::IHttpClient> client,
        httpcl::Config httpConfig = {},
        uint32_t serverIndex = 0);

    std::vector<uint8_t> callMethod(
        zserio::StringView methodName,
        zserio::IServiceData const& requestData,
        void* context) override;

private:
    OpenAPIClient client_;
};

}
