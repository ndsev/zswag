#pragma once

#include <istream>

#include "zswagcl/private/openapi-config.hpp"
#include "httpcl/http-client.hpp"
#include "httpcl/http-settings.hpp"

namespace zswagcl
{

/**
 * Download and parse OpenAPI config from URL.
 *
 * Throws on error.
 */
OpenAPIConfig fetchOpenAPIConfig(const std::string& url,
                                 httpcl::IHttpClient& client,
                                 httpcl::Config httpConfig = {});

/**
 * Parse OpenAPI config from input-stream.
 *
 * Throws on error.
 */
OpenAPIConfig parseOpenAPIConfig(std::istream&);

}
