#pragma once

#include <zserio/IService.h>

#include "private/openapi-client.hpp"
#include "httpcl/http-client.hpp"

namespace zswagcl
{

/**
 * Adapt generated zserio service calls to the selected OpenAPI server/transport.
 * Owns the transport and resolves request parameters through generated reflection;
 * request APIs must be generated with type-info support. Concurrent calls require
 * a concurrent transport (such as CurlHttpClient), independently readable request
 * data and a client lifetime covering all calls.
 */
class OAClient : public ::zserio::IServiceClient
{
public:
    /** Take ownership of the transport; serverIndex selects one advertised server URL. */
    OAClient(
        OpenAPIConfig config,
        std::unique_ptr<httpcl::IHttpClient> client,
        httpcl::Config httpConfig = {},
        uint32_t serverIndex = 0);

    /**
     * Synchronously serialize the requested fields/body and return raw response bytes.
     * Missing reflection/fields and HTTP failures propagate as exceptions. The
     * zserio context argument is currently unused, and requestData is borrowed only
     * for this invocation.
     */
    std::vector<uint8_t> callMethod(
        zserio::StringView methodName,
        zserio::IServiceData const& requestData,
        void* context) override;

private:
    OpenAPIClient client_;
};

}
