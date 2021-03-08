// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
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
    OpenAPIConfig config;

    OpenAPIClient(OpenAPIConfig config,
                  std::unique_ptr<httpcl::IHttpClient> client);
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
};

}
