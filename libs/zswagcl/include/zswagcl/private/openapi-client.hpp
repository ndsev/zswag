#pragma once

#include <memory>

#include "openapi-parser.hpp"
#include "openapi-config.hpp"
#include "openapi-parameter-helper.hpp"
#include "openapi-security.hpp"

#include "httpcl/uri.hpp"
#include "httpcl/http-client.hpp"

namespace zswagcl
{

/**
 * Resolve an OpenAPI operation into a blocking request through an owned HTTP client.
 * Concurrent calls are supported when the injected transport permits them (as
 * CurlHttpClient does), callbacks use independent request data, and config_/
 * httpConfig_ are not mutated. Keep the client alive until every call returns.
 */
class OpenAPIClient
{
public:
    OpenAPIConfig config_;
    httpcl::Config httpConfig_;
    AuthRegistry authHandlers_;

    /** Own the transport and snapshot the selected server; invalid index/URI throws. */
    OpenAPIClient(
        OpenAPIConfig config,
        httpcl::Config httpConfig,
        std::unique_ptr<httpcl::IHttpClient> client,
        uint32_t serverIndex = 0);
    ~OpenAPIClient();

    /**
     * Resolve parameters and authentication, then wait for the complete response.
     *
     * `fun` runs synchronously for path, query and header parameters. A non-GET
     * operation with a request body also calls it with an empty identifier and
     * ZSERIO_REQUEST_PART_WHOLE. ParameterValueHelper references are borrowed
     * only for the callback. The subsequent HTTP worker is joined before return.
     *
     * Unknown/unsupported methods, parameter resolution and unsatisfied security
     * requirements can throw before transport. Only HTTP 200 returns content;
     * every other status, including another 2xx, raises IHttpClient::Error.
     *
     * @param method  OpenAPI method identifier.
     * @param fun     Parameter resolve function.
     * @return Owned response bytes, without decoding or content-type validation.
     */
    std::string call(const std::string& method,
                     const std::function<ParameterValue(const std::string&, /* parameter identifier */
                                                        const std::string&, /* zserio request part path */
                                                        ParameterValueHelper&)>& fun);

private:
    std::unique_ptr<httpcl::IHttpClient> client_;
    httpcl::Settings settings_;
    httpcl::URIComponents server_;
};

}
