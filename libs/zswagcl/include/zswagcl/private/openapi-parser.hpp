#pragma once

#include <istream>

#include "zswagcl/private/openapi-config.hpp"
#include "httpcl/http-client.hpp"
#include "httpcl/http-settings.hpp"

namespace zswagcl
{

/**
 * Fetch a complete OpenAPI document synchronously through the borrowed client.
 * Persistent URL settings are merged into httpConfig before optional OAuth token
 * acquisition. The transport runs on a worker that is joined before return.
 *
 * Any 2xx response is parsed; other statuses raise IHttpClient::Error with the
 * response attached. URI/YAML/configuration errors propagate. Relative servers
 * are resolved against the document URL; an absent/empty server list becomes its
 * origin root. Reuse this client in OAClient to retain transport connections.
 */
OpenAPIConfig fetchOpenAPIConfig(const std::string& url,
                                 httpcl::IHttpClient& client,
                                 httpcl::Config httpConfig = {});

/**
 * Parse OpenAPI configuration from the remaining input stream.
 * Retain its text in OpenAPIConfig::content.
 * Parses the zswag subset: GET/POST/PUT/DELETE operations, explicitly mapped
 * parameters, zserio bodies and supported security schemes. This is not full
 * OpenAPI/JSON Schema validation or $ref resolution. YAML/config errors propagate.
 *
 * Without a document URL, relative servers remain unresolved and an absent
 * server list stays empty. Use fetchOpenAPIConfig for remote URL resolution.
 */
OpenAPIConfig parseOpenAPIConfig(std::istream&);

}
