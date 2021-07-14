#include "openapi-parser.hpp"

#include "httpcl/http-settings.hpp"
#include "httpcl/uri.hpp"

#include "yaml-cpp/yaml.h"
#include "stx/format.h"
#include <httplib.h>

#include <sstream>
#include <string>

using namespace std::string_literals;

namespace zswagcl
{

static auto parseParameterLocation(const YAML::Node& inNode)
{
    auto str = inNode.as<std::string>();
    if (str == "query")
        return OpenAPIConfig::ParameterLocation::Query;
    else if (str == "path")
        return OpenAPIConfig::ParameterLocation::Path;
    else if (str == "header")
        return OpenAPIConfig::ParameterLocation::Header;

    throw std::runtime_error("Unsupported parameter location");
}

/**
 * JSON schema is _not_ supported. The only field that is respected is
 * the 'format' field.
 */
static auto parseParameterSchema(const YAML::Node& schemaNode)
{
    if (auto formatNode = schemaNode["format"]) {
        auto format = formatNode.as<std::string>();

        if (format == "string")
            return OpenAPIConfig::Parameter::String;
        if (format == "byte" || format == "base64")
            return OpenAPIConfig::Parameter::Base64;
        if (format == "base64url")
            return OpenAPIConfig::Parameter::Base64url;
        if (format == "hex")
            return OpenAPIConfig::Parameter::Hex;
        if (format == "binary")
            return OpenAPIConfig::Parameter::Binary;

        throw std::runtime_error(stx::format("Unsupported format {}", format));
    }

    return OpenAPIConfig::Parameter::String;
}


/**
 * OpenAPI parameter style.
 *
 * Documentation: https://swagger.io/specification/#parameter-style
 */
static void parseParameterStyle(const YAML::Node& styleNode,
                                OpenAPIConfig::Parameter& parameter)
{
    /* Set default style for parameter location */
    switch (parameter.location) {
    case OpenAPIConfig::ParameterLocation::Header:
        parameter.style = OpenAPIConfig::Parameter::Form;
        parameter.explode = false;
        break;
    case OpenAPIConfig::ParameterLocation::Query:
        parameter.style = OpenAPIConfig::Parameter::Form;
        parameter.explode = true;
        break;
    case OpenAPIConfig::ParameterLocation::Path:
        parameter.style = OpenAPIConfig::Parameter::Simple;
        parameter.explode = false;
        break;
    }

    if (styleNode) {
        const auto& styleStr = styleNode.as<std::string>();
        if (styleStr == "matrix") {
            // TODO: Make sure location is path
            parameter.style = OpenAPIConfig::Parameter::Matrix;
        }
        if (styleStr == "label") {
            // TODO: Make sure location is path
            parameter.style = OpenAPIConfig::Parameter::Label;
        }
        if (styleStr == "simple") {
            // TODO: Make sure location is path
            parameter.style = OpenAPIConfig::Parameter::Simple;
        }
        if (styleStr == "form") {
            // TODO: Make sure location is not path
            parameter.style = OpenAPIConfig::Parameter::Form;
        }
    }
}

static void parseParameterExplode(const YAML::Node& explodeNode,
                                  OpenAPIConfig::Parameter& parameter)
{
    if (explodeNode) {
        auto explodeBool = explodeNode.as<bool>();

        parameter.explode = explodeBool;
        // TODO: If explode, make sure location is query or path
    }
}

static bool parseMethodParameter(const YAML::Node& parameterNode,
                                 OpenAPIConfig::Path& path)
{
    auto nameNode = parameterNode["name"];
    if (!nameNode)
        throw std::runtime_error("Missing required node 'name'");

    auto& parameter = path.parameters[nameNode.as<std::string>()];
    parameter.ident = nameNode.as<std::string>();

    if (auto inNode = parameterNode["in"]) {
        parameter.location = parseParameterLocation(inNode);
    }

    if (auto zserioRequestPartNode = parameterNode["x-zserio-request-part"])
        parameter.field = zserioRequestPartNode.as<std::string>();
    else
        return false;

    if (auto schemaNode = parameterNode["schema"]) {
        parameter.format = parseParameterSchema(schemaNode);
    }

    parseParameterStyle(parameterNode["style"], parameter);
    parseParameterExplode(parameterNode["explode"], parameter);

    return true;
}

static void parseMethodBody(const YAML::Node& methodNode,
                            OpenAPIConfig::Path& path)
{
    if (auto bodyNode = methodNode["requestBody"]) {
        if (auto contentNode = bodyNode["content"]) {
            for (auto contentTypeNode : contentNode) {
                if (contentTypeNode.first.as<std::string>() != ZSERIO_OBJECT_CONTENT_TYPE)
                    throw std::runtime_error("Unsupported body content type");
                path.bodyRequestObject = true;
            }
        }
    }
}

static OpenAPIConfig::SecurityAlternatives parseSecurity(
        const YAML::Node& securityNode,
        OpenAPIConfig const& config)
{
    OpenAPIConfig::SecurityAlternatives result;

    for (auto const& alternative : securityNode)
    {
        auto& newAlternativeAuthSet = result.emplace_back();

        for (auto const& requiredScheme : alternative) {
            auto scheme = config.securitySchemes.find(requiredScheme.first.as<std::string>());
            if (scheme != config.securitySchemes.end())
                newAlternativeAuthSet.emplace_back(scheme->second);
            // TODO: Else: Throw
        }

        // TODO: Throw if (newAlternativeAuthSet.empty())
    }
    return result;
}

static void parseMethod(const std::string& method,
                        const std::string& uriPath,
                        const YAML::Node& pathNode,
                        OpenAPIConfig& config)
{
    if (auto methodNode = pathNode[method]) {
        auto opIdNode = methodNode["operationId"];
        if (!opIdNode)
            throw std::runtime_error("Missing required field 'operationId'");

        auto& path = config.methodPath[opIdNode.as<std::string>()];
        path.path = uriPath;
        path.httpMethod = method;
        std::transform(path.httpMethod.begin(),
                       path.httpMethod.end(),
                       path.httpMethod.begin(),
                       &toupper);

        for (const auto& parameterNode : methodNode["parameters"]) {
            parseMethodParameter(parameterNode, path);
        }

        if (auto securityNode = methodNode["security"]) {
            path.security = parseSecurity(securityNode, config);
        }

        parseMethodBody(methodNode, path);
    }
}

static void parseSecurityScheme(
    const std::string& name,
    const YAML::Node& schemeNode,
    OpenAPIConfig& config)
{
    OpenAPIConfig::SecuritySchemePtr newScheme;
    auto schemeType = schemeNode["type"].as<std::string>();

    if (schemeType == "http") {
        auto schemeHttpType = schemeNode["scheme"].as<std::string>();
        if (schemeHttpType == "basic")
            newScheme = std::make_shared<OpenAPIConfig::BasicAuth>(name);
        else if (schemeHttpType == "bearer")
            newScheme = std::make_shared<OpenAPIConfig::BearerAuth>(name);
        // TODO: Else: Throw
    }
    else if (schemeType == "apiKey") {
        auto keyLocationString = schemeNode["in"].as<std::string>();
        auto parameterName = schemeNode["name"].as<std::string>();

        if (keyLocationString == "query")
            newScheme = std::make_shared<OpenAPIConfig::APIKeyAuth>(name, OpenAPIConfig::ParameterLocation::Query, parameterName);
        else if (keyLocationString == "header")
            newScheme = std::make_shared<OpenAPIConfig::APIKeyAuth>(name, OpenAPIConfig::ParameterLocation::Header, parameterName);
        else if (keyLocationString == "cookie")
            newScheme = std::make_shared<OpenAPIConfig::CookieAuth>(name, parameterName);
        // TODO: Else: Throw
    }
    // TODO: Else: Throw

    config.securitySchemes[name] = newScheme;
}

static void parsePath(const std::string& uriPath,
                      const YAML::Node& pathNode,
                      OpenAPIConfig& config)
{
    static const char* supportedMethods[] = {
        "get", "post", "put", "patch", "delete"
    };

    for (const auto method : supportedMethods) {
        parseMethod(method, uriPath, pathNode, config);
    }
}

static void parseServer(const YAML::Node& serverNode,
                        OpenAPIConfig& config)
{
    if (auto urlNode = serverNode["url"]) {
        auto urlStr = urlNode.as<std::string>();
        if (urlStr.empty()) {
            // Ignore empty URLs.
        } else if (urlStr.front() == '/') {
            config.uri = httpcl::URIComponents::fromStrPath(urlStr);
        } else {
            config.uri = httpcl::URIComponents::fromStrRfc3986(urlStr);
        }
    }
}

OpenAPIConfig parseOpenAPIConfig(std::istream& s)
{
    OpenAPIConfig config;

    auto doc = YAML::Load(s);
    if (auto servers = doc["servers"]) {
        auto first = servers.begin();
        if (first != servers.end()) {
            try { parseServer(*first, config); }
            catch (const httpcl::URIError& e) {
                throw std::runtime_error(std::string("OpenAPI spec doesn't contain a valid server config - details: ")
                                         .append(e.what()));
            }
        }
    }

    if (auto components = doc["components"]) {
        if (auto securitySchemes = components["securitySchemes"]) {
            for (auto const& scheme : securitySchemes) {
                parseSecurityScheme(
                    scheme.first.as<std::string>(),
                    scheme.second, config);
            }
        }
    }

    if (auto security = doc["security"]) {
        config.defaultSecurityScheme = parseSecurity(security, config);
    }

    if (auto paths = doc["paths"]) {
        for (const auto& path : paths) {
            parsePath(path.first.as<std::string>(), path.second, config);
        }
    } else {
        throw std::runtime_error("Missing required node 'paths'");
    }

    return config;
}

OpenAPIConfig fetchOpenAPIConfig(const std::string& url,
                                 httpcl::IHttpClient& client)
{
    // Load client config content.
    auto uriParts = httpcl::URIComponents::fromStrRfc3986(url);
    auto res = client.get(uriParts.build(), {});

    // Create client from loaded config JSON.
    if (res.status >= 200 && res.status < 300) {
        std::stringstream ss(res.content, std::ios_base::in);

        auto config = parseOpenAPIConfig(ss);
        if (config.uri.scheme.empty())
            config.uri.scheme = uriParts.scheme;
        if (config.uri.host.empty()) {
            config.uri.host = uriParts.host;
            config.uri.port = uriParts.port;
        }

        return config;
    }

    // FIXME: Python code is parsing the HTTP status from exception message!
    //        Pybind11 does not support custom exception types yet.
    throw httpcl::IHttpClient::Error(
        res,
        stx::format(
            "Error configuring OpenAPI service from URI: '{}', status: {}, content: '{}'",
            url,
            res.status,
            res.content));
}

}
