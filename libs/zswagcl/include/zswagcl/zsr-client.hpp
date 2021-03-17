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
              std::unique_ptr<httpcl::IHttpClient> client);

    ~ZsrClient();

    void callMethod(const std::string& methodName,
                    const std::vector<uint8_t>& requestData,
                    std::vector<uint8_t>& responseData,
                    void* context) override;

private:
    OpenAPIClient client_;
};

}
