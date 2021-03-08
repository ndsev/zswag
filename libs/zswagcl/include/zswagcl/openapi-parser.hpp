// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
#pragma once

#include <istream>

#include "zswagcl/openapi-config.hpp"
#include "httpcl/http-client.hpp"

namespace zswagcl
{

/**
 * Download and parse OpenAPI config from URL.
 *
 * Throws on error.
 */
OpenAPIConfig fetchOpenAPIConfig(const std::string& url,
                                 httpcl::IHttpClient& client);

/**
 * Parse OpenAPI config from input-stream.
 *
 * Throws on error.
 */
OpenAPIConfig parseOpenAPIConfig(std::istream&);

}
