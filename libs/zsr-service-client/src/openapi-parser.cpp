// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
#include "openapi-parser.hpp"

#include "http-client/http-settings.hpp"
#include "http-client/uri.hpp"

#include "yaml-cpp/yaml.h"
#include <httplib.h>

#include <sstream>
#include <string>

using namespace std::string_literals;

namespace zsr_service
{

static auto parseParameterLocation(const YAML::Node& inNode)
{
    auto str = inNode.as<std::string>();
    if (str == "query")
        return HTTPService::Config::Parameter::Query;
    else if (str == "path")
        return HTTPService::Config::Parameter::Path;

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

        if (format == "byte" || format == "base64")
            return HTTPService::Config::Parameter::Base64;
        if (format == "hex")
            return HTTPService::Config::Parameter::Hex;
        if (format == "binary")
            return HTTPService::Config::Parameter::Binary;

        throw std::runtime_error("Unsupported format");
    }

    return HTTPService::Config::Parameter::String;
}


/**
 * OpenAPI parameter style.
 *
 * Documentation: https://swagger.io/specification/#parameter-style
 */
static void parseParameterStyle(const YAML::Node& styleNode,
                                HTTPService::Config::Parameter& parameter)
{
    /* Set default style for parameter location */
    switch (parameter.location) {
    case HTTPService::Config::Parameter::Query:
        parameter.style = HTTPService::Config::Parameter::Form;
        break;
    case HTTPService::Config::Parameter::Path:
        parameter.style = HTTPService::Config::Parameter::Simple;
        break;
    }

    if (styleNode) {
        const auto& styleStr = styleNode.as<std::string>();
        if (styleStr == "matrix")
            parameter.style = HTTPService::Config::Parameter::Matrix;
        if (styleStr == "form")
            parameter.style = HTTPService::Config::Parameter::Form;
        if (styleStr == "simple")
            parameter.style = HTTPService::Config::Parameter::Simple;
    }
}

static void parseParameterExplode(const YAML::Node& explodeNode,
                                  HTTPService::Config::Parameter& parameter)
{
    if (parameter.style == HTTPService::Config::Parameter::Matrix)
        parameter.explode = true;
    else
        parameter.explode = false;

    if (explodeNode) {
        auto explodeBool = explodeNode.as<bool>();

        parameter.explode = explodeBool;
    }
}

static bool parseMethodParameter(const YAML::Node& parameterNode,
                                 HTTPService::Config::Path& path)
{
    auto nameNode = parameterNode["name"];
    if (!nameNode)
        throw std::runtime_error("Missing required node 'name'");

    auto& parameter = path.parameters[nameNode.as<std::string>()];
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
                            HTTPService::Config::Path& path)
{
    if (auto bodyNode = methodNode["requestBody"]) {
        if (auto contentNode = bodyNode["content"]) {
            for (auto contentTypeNode : contentNode) {
                if (contentTypeNode.first.as<std::string>() != "application/x-zserio-object")
                    throw std::runtime_error("Unsupported body content type");
            }
        }
    }
}

static void parseMethod(const std::string& method,
                        const std::string& uriPath,
                        const YAML::Node& pathNode,
                        HTTPService::Config& config)
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
                      HTTPService::Config& config)
{
    static const char* supportedMethods[] = {
        "get", "post", "put", "patch", "delete"
    };

    for (const auto method : supportedMethods) {
        parseMethod(method, uriPath, pathNode, config);
    }
}

static void parseServer(const YAML::Node& serverNode,
                        HTTPService::Config& config)
{
    if (auto urlNode = serverNode["url"]) {
        auto urlStr = urlNode.as<std::string>();
        if (urlStr.empty()) {
            // Ignore empty URLs.
        } else if (urlStr.front() == '/') {
            config.uri = ndsafw::URIComponents::fromStrPath(urlStr);
        } else {
            config.uri = ndsafw::URIComponents::fromStrRfc3986(urlStr);
        }
    }
}

HTTPService::Config parseOpenAPIConfig(std::istream& s)
{
    HTTPService::Config config;

    auto doc = YAML::Load(s);
    if (auto servers = doc["servers"]) {
        auto first = servers.begin();
        if (first != servers.end()) {
            try { parseServer(*first, config); }
            catch (const ndsafw::URIError& e) {
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

HTTPService::Config fetchOpenAPIConfig(const std::string& url,
                                       ndsafw::IHttpClient& client)
{
    // Load client config content
    auto uriParts = ndsafw::URIComponents::fromStrRfc3986(url);
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
