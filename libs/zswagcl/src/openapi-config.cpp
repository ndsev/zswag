#include <regex>

#include "zswagcl/private/openapi-config.hpp"
#include "zswagcl/export.hpp"
#include "stx/format.h"

namespace zswagcl
{

ZSWAGCL_EXPORT const std::string ZSERIO_OBJECT_CONTENT_TYPE = "application/x-zserio-object";
ZSWAGCL_EXPORT const std::string ZSERIO_REQUEST_PART = "x-zserio-request-part";
ZSWAGCL_EXPORT const std::string ZSERIO_REQUEST_PART_WHOLE = "*";

bool OpenAPIConfig::BasicAuth::checkOrApply(httpcl::Config& config, std::string& err) const {
    if (config.auth.has_value())
        return true;

    std::regex basicAuthValueRe{
        "^Basic .+$",
        std::regex_constants::ECMAScript|std::regex_constants::icase
    };

    auto found = std::any_of(config.headers.begin(), config.headers.end(), [&](auto const& headerNameAndValue){
        return headerNameAndValue.first == "Authorization" &&
               std::regex_match(headerNameAndValue.second, basicAuthValueRe);
    });

    if (found)
        return true;

    err = "HTTP basic-auth credentials are missing.";
    return false;
}

bool OpenAPIConfig::APIKeyAuth::checkOrApply(httpcl::Config& config, std::string& err) const {

    switch (location) {
        case ParameterLocation::Query:
            if (config.query.find(keyName) != config.query.end())
                return true;
            if (config.apiKey) {
                config.query.insert({keyName, *config.apiKey});
                return true;
            }
            err = stx::format("Neither api-key nor query parameter `{}` is set.", keyName);
            return false;
        case ParameterLocation::Header:
            if (config.headers.find(keyName) != config.headers.end())
                return true;
            if (config.apiKey) {
                config.headers.insert({keyName, *config.apiKey});
                return true;
            }
            err = stx::format("Neither api-key nor header `{}` is set.", keyName);
            return false;
        default:
            err = stx::format("Unsupported API-key location.");
            return false;
    }
}

bool OpenAPIConfig::CookieAuth::checkOrApply(httpcl::Config& config, std::string& err) const {
    if (config.cookies.find(cookieName) != config.cookies.end())
        return true;
    if (config.apiKey) {
        config.cookies.insert({cookieName, *config.apiKey});
        return true;
    }
    err = stx::format("Neither api-key nor cookie `{}` is set.", cookieName);
    return false;
}

bool OpenAPIConfig::BearerAuth::checkOrApply(httpcl::Config& config, std::string& err) const {
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