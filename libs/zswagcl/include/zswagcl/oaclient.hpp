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
        zswagcl::OpenAPIConfig config,
        std::unique_ptr<httpcl::IHttpClient> client,
        httpcl::Config httpConfig = {});

    std::vector<uint8_t> callMethod(
        zserio::StringView methodName,
        zserio::IServiceData const& requestData,
        void* context) override;

private:
    OpenAPIClient client_;
};

}
