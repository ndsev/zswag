#include <regex>

#include "openapi-config.hpp"
#include "stx/format.h"

namespace zswagcl
{

const std::string ZSERIO_OBJECT_CONTENT_TYPE = "application/x-zserio-object";
const std::string ZSERIO_REQUEST_PART = "x-zserio-request-part";
const std::string ZSERIO_REQUEST_PART_WHOLE = "*";

bool OpenAPIConfig::BasicAuth::check(httpcl::Config const& config, std::string& err) const {
    if (config.auth.has_value())
        return true;
    err = "HTTP basic-auth credentials are missing.";
    return false;
}

bool OpenAPIConfig::APIKeyAuth::check(httpcl::Config const& config, std::string& err) const {
    switch (location) {
        case ParameterLocation::Query:
            if (config.query.find(keyName) != config.query.end())
                return true;
            err = stx::format("Query parameter `{}` is missing.", keyName);
            return false;
        case ParameterLocation::Header:
            if (config.headers.find(keyName) != config.headers.end())
                return true;
            err = stx::format("Header `{}` is missing.", keyName);
            return false;
        default:
            err = stx::format("Unsupported API-key location.");
            return false;
    }
}

bool OpenAPIConfig::CookieAuth::check(httpcl::Config const& config, std::string& err) const {
    if (config.cookies.find(cookieName) != config.cookies.end())
        return true;
    err = stx::format("Cookie `{}` is missing.", cookieName);
    return false;
}

bool OpenAPIConfig::BearerAuth::check(httpcl::Config const& config, std::string& err) const {
    std::regex bearerValueRe{
        "^Bearer .+$",
        std::regex_constants::ECMAScript|std::regex_constants::icase
    };

    auto found = std::any_of(config.headers.begin(), config.headers.end(), [&](auto const& headerNameAndValue){
        return headerNameAndValue.first == "Authorization" &&
               std::regex_match(headerNameAndValue.second, bearerValueRe);
    });

    if (found)
        return true;
    err = "Header `Authorization: Bearer ...` is missing.";
    return false;
}

OpenAPIConfig::SecurityScheme::SecurityScheme(std::string id) :
    id(std::move(id))
{}

OpenAPIConfig::BasicAuth::BasicAuth(std::string id) :
        SecurityScheme(std::move(id))
{}

OpenAPIConfig::BearerAuth::BearerAuth(std::string id) :
        SecurityScheme(std::move(id))
{}

OpenAPIConfig::APIKeyAuth::APIKeyAuth(std::string id, ParameterLocation location, std::string keyName) :
        SecurityScheme(std::move(id)),
        location(location),
        keyName(std::move(keyName))
{}

OpenAPIConfig::CookieAuth::CookieAuth(std::string id, std::string cookieName) :
        SecurityScheme(std::move(id)),
        cookieName(std::move(cookieName))
{}

}