// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
#include "openapi-parser.hpp"

#include "httpcl/http-settings.hpp"
#include "httpcl/uri.hpp"

#include "yaml-cpp/yaml.h"
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
        return OpenAPIConfig::Parameter::Query;
    else if (str == "path")
        return OpenAPIConfig::Parameter::Path;

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

        throw std::runtime_error("Unsupported format");
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
    case OpenAPIConfig::Parameter::Query:
        parameter.style = OpenAPIConfig::Parameter::Form;
        parameter.explode = true;
        break;
    case OpenAPIConfig::Parameter::Path:
        parameter.style = OpenAPIConfig::Parameter::Simple;
        parameter.explode = false;
        break;
    }

    if (styleNode) {
        const auto& styleStr = styleNode.as<std::string>();
        if (styleStr == "matrix")
            parameter.style = OpenAPIConfig::Parameter::Matrix;
        if (styleStr == "label")
            parameter.style = OpenAPIConfig::Parameter::Label;
        if (styleStr == "form")
            parameter.style = OpenAPIConfig::Parameter::Form;
        if (styleStr == "simple")
            parameter.style = OpenAPIConfig::Parameter::Simple;
    }
}

static void parseParameterExplode(const YAML::Node& explodeNode,
                                  OpenAPIConfig::Parameter& parameter)
{
    if (explodeNode) {
        auto explodeBool = explodeNode.as<bool>();

        parameter.explode = explodeBool;
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
            }
        }
    }
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

        parseMethodBody(methodNode, path);
    }
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
    // Load client config content
    auto uriParts = httpcl::URIComponents::fromStrRfc3986(url);
    auto res = client.get(uriParts.build());

    // Create client from loaded config JSON
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

    throw std::runtime_error("Error configuring service; http-status: "s +
                             std::to_string(res.status));
}

}
