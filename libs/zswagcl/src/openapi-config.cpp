#include <regex>

#include "openapi-config.hpp"
#include "stx/string.h"

namespace zswagcl
{

const std::string ZSERIO_OBJECT_CONTENT_TYPE = "application/x-zserio-object";
const std::string ZSERIO_REQUEST_PART = "x-zserio-request-part";
const std::string ZSERIO_REQUEST_PART_WHOLE = "*";

bool OpenAPIConfig::BasicAuth::check(httpcl::Config const& config) const {
    return config.auth.has_value();
}

bool OpenAPIConfig::APIKeyAuth::check(httpcl::Config const& config) const {
    switch (location) {
        case ParameterLocation::Query:
            return config.query.find(keyName) != config.query.end();
        case ParameterLocation::Header:
            return config.headers.find(keyName) != config.headers.end();
        default:
            return false;
    }
}

bool OpenAPIConfig::CookieAuth::check(httpcl::Config const& config) const {
    return config.cookies.find(cookieName) != config.cookies.end();
}

bool OpenAPIConfig::BearerAuth::check(httpcl::Config const& config) const {
    std::regex bearerValueRe{
        "^Bearer .+$",
        std::regex_constants::ECMAScript|std::regex_constants::icase
    };

    for (auto const& [headerName, headerValue] : config.headers) {
        if (headerName == "Authorization" && std::regex_match(headerValue, bearerValueRe)) {
            return true;
        }
    }

    return false;
}

}
