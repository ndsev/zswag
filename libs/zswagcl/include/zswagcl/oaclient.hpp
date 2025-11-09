#pragma once

#include <string_view>
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

    zserio::Vector<uint8_t> callMethod(
        std::string_view methodName,
        zserio::IServiceData const& requestData,
        void* context) override;

private:
    OpenAPIClient client_;
};

}
