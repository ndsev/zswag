#pragma once

#include <zserio/IService.h>

#include "openapi-client.hpp"
#include "httpcl/http-client.hpp"

namespace zswagcl
{

class ZsrClient : public ::zserio::IService
{
public:
    ZsrClient(zswagcl::OpenAPIConfig config,
              std::unique_ptr<httpcl::IHttpClient> client,
              httpcl::Config httpConfig = {});

    void callMethod(zserio::StringView methodName,
                    zserio::Span<const uint8_t> requestData,
                    zserio::IBlobBuffer& responseData,
                    void* context) override;

private:
    OpenAPIClient client_;
};

}
