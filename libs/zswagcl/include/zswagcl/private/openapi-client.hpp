#pragma once

#include <memory>

#include "openapi-parser.hpp"
#include "openapi-config.hpp"
#include "openapi-parameter-helper.hpp"

#include "httpcl/uri.hpp"
#include "httpcl/http-client.hpp"

namespace zswagcl
{

class OpenAPIClient
{
public:
    OpenAPIConfig config_;
    httpcl::Config httpConfig_;

    OpenAPIClient(OpenAPIConfig config,
                  httpcl::Config httpConfig,
                  std::unique_ptr<httpcl::IHttpClient> client,
                  uint32_t serverIndex = 0);
    ~OpenAPIClient();

    /**
     * Call OpenAPI method.
     *
     * The callback `fun` is called for each URL and request parameter of the
     * method.
     *
     * @param method  OpenAPI method identifier.
     * @param fun     Parameter resolve function.
     * @return Response buffer.
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
