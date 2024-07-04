#include "private/openapi-parser.hpp"

#include "httpcl/http-settings.hpp"
#include "httpcl/uri.hpp"

#include "yaml-cpp/yaml.h"
#include "stx/format.h"
#include "httpcl/log.hpp"
#include <httplib.h>
#include <future>

#include <sstream>
#include <string>

using namespace std::string_literals;

namespace {

struct YAMLScope {
    std::string name_;
    YAMLScope const* parent_ = nullptr;
    YAML::Node node_;

    explicit YAMLScope(std::string name, YAML::Node const& n, YAMLScope const* parent = nullptr)
        : name_(std::move(name)), node_(n), parent_(parent)
    {}

    std::string str() const {
        std::string result;
        if (parent_)
            result = parent_->str() + ".";
        else
            result = "$";
        result += name_;
        return result;
    }

    std::runtime_error valueError(std::string const& value, std::vector<std::string> const& allowed) const {
        return httpcl::logRuntimeError(stx::format(
            "ERROR while parsing OpenAPI schema:\n"
            "    At {}:\n"
            "        Unsupported value `{}`.\n"
            "        Allowed values are:\n"
            "        - {}\n",
            str(),
            value,
            stx::join(allowed.begin(), allowed.end(), "\n        - ")));
    }

    std::runtime_error contextualValueError(std::string const& reason, std::string const& value, std::vector<std::string> const& allowed) const {
        return httpcl::logRuntimeError(stx::format(
            "ERROR while parsing OpenAPI schema:\n"
            "    At {}:\n"
            "        Because {}: Value `{}` is not allowed.\n"
            "        Allowed values are:\n"
            "        - {}\n",
            str(),
            reason,
            value,
            stx::join(allowed.begin(), allowed.end(), "\n        - ")));
    }

    std::runtime_error missingFieldError(std::string const& field) const {
        return httpcl::logRuntimeError(stx::format(
            "ERROR while parsing OpenAPI schema:\n"
            "    At {}:\n"
            "        Mandatory field `{}` is missing.\n",
            str(),
            field));
    }

    operator bool() const {
        return node_.operator bool();
    }

    YAMLScope operator[] (char const* name) const {
        auto child = node_[name];
        return YAMLScope(name, child, this);
    }

    YAMLScope operator[] (std::string const& name) const {
        return operator[](name.c_str());
    }

    YAMLScope mandatoryChild(std::string const& name) const {
        auto result = operator[](name);
        if (!result)
            throw missingFieldError(name);
        return result;
    }

    template<typename T>
    T as() const {
        return node_.as<T>();
    }

    void forEach(std::function<void(YAMLScope const& child)> const& fun) {
        if (!node_ || !fun || !(node_.IsMap() || node_.IsSequence()))
            return;
        size_t i = 0;
        for (auto const& child : node_) {
            if (node_.IsMap())
                fun(YAMLScope(child.first.as<std::string>(), child.second, this));
            else
                fun(YAMLScope(stx::to_string(i), child, this));
            ++i;
        }
    }
};

}

namespace zswagcl
{

static auto parseParameterLocation(YAMLScope const& inNode)
{
    auto str = inNode.as<std::string>();
    if (str == "query")
        return OpenAPIConfig::ParameterLocation::Query;
    else if (str == "path")
        return OpenAPIConfig::ParameterLocation::Path;
    else if (str == "header")
        return OpenAPIConfig::ParameterLocation::Header;

    throw inNode.valueError(str, {"query", "path", "header"});
}

/**
 * JSON schema is _not_ supported. The only field that is respected is
 * the 'format' field.
 */
static auto parseParameterSchema(YAMLScope const& schemaNode)
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

        throw formatNode.valueError(format, {
            "string",
            "byte",
            "base64",
            "base64url",
            "hex",
            "binary"
        });
    }

    return OpenAPIConfig::Parameter::String;
}


/**
 * OpenAPI parameter style.
 *
 * Documentation: https://swagger.io/specification/#parameter-style
 */
static void parseParameterStyle(YAMLScope const& styleNode,
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
            if (parameter.location != OpenAPIConfig::ParameterLocation::Path)
                throw styleNode.contextualValueError(" in != `path`", styleStr, {"form"});
            parameter.style = OpenAPIConfig::Parameter::Matrix;
        }
        if (styleStr == "label") {
            if (parameter.location != OpenAPIConfig::ParameterLocation::Path)
                throw styleNode.contextualValueError(" in != `path`", styleStr, {"form"});
            parameter.style = OpenAPIConfig::Parameter::Label;
        }
        if (styleStr == "simple") {
            if (parameter.location != OpenAPIConfig::ParameterLocation::Path)
                throw styleNode.contextualValueError(" in != `path`", styleStr, {"form"});
            parameter.style = OpenAPIConfig::Parameter::Simple;
        }
        if (styleStr == "form") {
            if (parameter.location == OpenAPIConfig::ParameterLocation::Path)
                throw styleNode.contextualValueError(" in == `path`", styleStr, {"matrix", "label", "simple"});
            parameter.style = OpenAPIConfig::Parameter::Form;
        }
    }
}

static void parseParameterExplode(YAMLScope const& explodeNode,
                                  OpenAPIConfig::Parameter& parameter)
{
    if (explodeNode) {
        auto explodeBool = explodeNode.as<bool>();

        parameter.explode = explodeBool;

        // If explode, make sure location is query or path
        if (parameter.explode &&
            parameter.location != OpenAPIConfig::ParameterLocation::Query &&
            parameter.location != OpenAPIConfig::ParameterLocation::Path)
        {
            throw explodeNode.contextualValueError(
                ".location != `query` && .location != `path`",
                "true",
                {"false"});
        }
    }
}

static void parseMethodParameter(YAMLScope const& parameterNode,
                                 OpenAPIConfig::Path& path)
{
    auto nameNode = parameterNode.mandatoryChild("name");

    if (!parameterNode[ZSERIO_REQUEST_PART]) {
        // Ignore parameters which do not have x-zserio-request-part, but
        // output a warning for such parameters if they are not optional.
        // By default, OpenAPI treats all request parameters as optional.
        // You can add required: true to mark a parameter as required.
        if (auto requiredNode = parameterNode["required"]) {
            if (requiredNode.as<bool>())
                httpcl::log().warn(
                    "The parameter {} does not have x-zserio-request-part and is not optional."
                    "Ensure that it is filled by passing additional HTTP settings.",
                    parameterNode.str());
        }
        return;
    }

    auto& parameter = path.parameters[nameNode.as<std::string>()];
    parameter.ident = nameNode.as<std::string>();

    if (auto inNode = parameterNode["in"]) {
        parameter.location = parseParameterLocation(inNode);
    }

    parameter.field = parameterNode.mandatoryChild(ZSERIO_REQUEST_PART).as<std::string>();

    if (auto schemaNode = parameterNode["schema"]) {
        parameter.format = parseParameterSchema(schemaNode);
    }

    parseParameterStyle(parameterNode["style"], parameter);
    parseParameterExplode(parameterNode["explode"], parameter);
}

static void parseMethodBody(YAMLScope const& methodNode,
                            OpenAPIConfig::Path& path)
{
    if (auto bodyNode = methodNode["requestBody"]) {
        if (auto contentNode = bodyNode["content"]) {
            for (auto contentTypeNode : contentNode.node_) {
                auto contentType = contentTypeNode.first.as<std::string>();
                if (contentType == ZSERIO_OBJECT_CONTENT_TYPE)
                    path.bodyRequestObject = true;
                else
                    httpcl::log().debug("Ignoring request body MIME type '{}'.", contentType);
            }
        }
    }
}

static OpenAPIConfig::SecurityAlternatives parseSecurity(
        YAMLScope const& securityNode,
        OpenAPIConfig const& config)
{
    OpenAPIConfig::SecurityAlternatives result;

    for (auto const& alternative : securityNode.node_)
    {
        auto& newAlternativeAuthSet = result.emplace_back();

        for (auto const& requiredScheme : alternative) {
            auto schemeName = requiredScheme.first.as<std::string>();
            auto scheme = config.securitySchemes.find(schemeName);
            if (scheme != config.securitySchemes.end())
                newAlternativeAuthSet.emplace_back(scheme->second);
            else {
                std::vector<std::string> schemeNames;
                std::transform(config.securitySchemes.begin(),
                               config.securitySchemes.end(),
                               std::back_inserter(schemeNames),
                               [](auto const& kv){return kv.first;});
                throw securityNode.valueError(schemeName, schemeNames);
            }
        }

        if (newAlternativeAuthSet.empty())
            throw securityNode.valueError("<empty>", {"<non-empty dictionary with scheme-name keys>"});
    }
    return result;
}

static void parseMethod(const std::string& method,
                        const YAMLScope& pathNode,
                        OpenAPIConfig& config)
{
    if (auto methodNode = pathNode[method]) {
        auto opIdNode = methodNode.mandatoryChild("operationId");

        auto& path = config.methodPath[opIdNode.as<std::string>()];
        path.path = pathNode.name_;
        path.httpMethod = method;
        std::transform(path.httpMethod.begin(),
                       path.httpMethod.end(),
                       path.httpMethod.begin(),
                       &toupper);

        methodNode["parameters"].forEach([&](auto const& parameterNode){
            parseMethodParameter(parameterNode, path);
        });

        if (auto securityNode = methodNode["security"])
            path.security = parseSecurity(securityNode, config);

        parseMethodBody(methodNode, path);
    }
}

static void parseSecurityScheme(
    const YAMLScope& schemeNode,
    OpenAPIConfig& config)
{
    auto& name = schemeNode.name_;
    OpenAPIConfig::SecuritySchemePtr newScheme;
    auto schemeTypeNode = schemeNode.mandatoryChild("type");
    auto schemeType = schemeTypeNode.as<std::string>();

    if (schemeType == "http") {
        auto schemeHttpTypeNode = schemeNode.mandatoryChild("scheme");
        auto schemeHttpType = schemeHttpTypeNode.as<std::string>();
        if (schemeHttpType == "basic")
            newScheme = std::make_shared<OpenAPIConfig::BasicAuth>(name);
        else if (schemeHttpType == "bearer")
            newScheme = std::make_shared<OpenAPIConfig::BearerAuth>(name);
        else
            throw schemeHttpTypeNode.valueError(schemeHttpType, {"basic", "bearer"});
    }
    else if (schemeType == "apiKey") {
        auto keyLocationNode = schemeNode.mandatoryChild("in");
        auto keyLocationString = keyLocationNode.as<std::string>();
        auto parameterNameNode = schemeNode.mandatoryChild("name");
        auto parameterName = parameterNameNode.as<std::string>();

        if (keyLocationString == "query")
            newScheme = std::make_shared<OpenAPIConfig::APIKeyAuth>(name, OpenAPIConfig::ParameterLocation::Query, parameterName);
        else if (keyLocationString == "header")
            newScheme = std::make_shared<OpenAPIConfig::APIKeyAuth>(name, OpenAPIConfig::ParameterLocation::Header, parameterName);
        else if (keyLocationString == "cookie")
            newScheme = std::make_shared<OpenAPIConfig::CookieAuth>(name, parameterName);
        else
            throw keyLocationNode.valueError(keyLocationString, {"query", "header", "cookie"});
    }
    else
        throw schemeTypeNode.valueError(schemeType, {"http", "apiKey"});

    config.securitySchemes[name] = newScheme;
}

static void parsePath(const YAMLScope& pathNode,
                      OpenAPIConfig& config)
{
    static const char* supportedMethods[] = {
        "get", "post", "put", "delete"
    };

    for (const auto method : supportedMethods) {
        parseMethod(method, pathNode, config);
    }
}

static void parseServer(const YAMLScope& serverNode,
                        OpenAPIConfig& config)
{
    if (auto urlNode = serverNode["url"]) {
        auto urlStr = urlNode.as<std::string>();
        if (urlStr.empty()) {
            // Ignore empty URLs.
        } else if (urlStr.front() == '/') {
            config.servers.emplace_back(httpcl::URIComponents::fromStrPath(urlStr));
        } else {
            config.servers.emplace_back(httpcl::URIComponents::fromStrRfc3986(urlStr));
        }
    }
}

OpenAPIConfig parseOpenAPIConfig(std::istream& ss)
{
    OpenAPIConfig config;
    config.content = std::string(std::istreambuf_iterator<char>(ss), {});

    auto doc = YAML::Load(config.content);
    YAMLScope docScope{"", doc};
    docScope["servers"].forEach([&](auto const& serverNode){
        try { parseServer(serverNode, config); }
        catch (const httpcl::URIError& e) {
            throw httpcl::logRuntimeError(
                stx::format("OpenAPI spec contains invalid server entry:\n    {}", e.what()));
        }
    });

    if (auto components = docScope["components"]) {
        components["securitySchemes"].forEach([&](auto const& scheme){
            parseSecurityScheme(scheme, config);
        });
    }

    if (auto security = docScope["security"]) {
        config.defaultSecurityScheme = parseSecurity(security, config);
    }

    docScope.mandatoryChild("paths").forEach([&](auto const& path){
        parsePath(path, config);
    });

    return config;
}

OpenAPIConfig fetchOpenAPIConfig(const std::string& url,
                                 httpcl::IHttpClient& client,
                                 httpcl::Config httpConfig)
{
    std::string debugContext = stx::format("[fetchOpenAPIConfig({})]", url);

    // Add persistent configuration
    httpcl::log().debug("{} Applying HTTP settings ...", debugContext);
    httpConfig |= httpcl::Settings()[url];

    // Load client config content.
    httpcl::log().debug("{} Parsing URL ...", debugContext);
    auto uriParts = httpcl::URIComponents::fromStrRfc3986(url);
    httpcl::log().debug("{} Executing HTTP GET ...", debugContext);
    auto resFuture = std::async(std::launch::async, [uriParts, httpConfig, &client] {
        return client.get(uriParts.build(), httpConfig);
    });
    while (resFuture.wait_for(std::chrono::seconds{1}) != std::future_status::ready)
        httpcl::log().debug("{} Waiting for response ...", debugContext);
    auto res = resFuture.get();
    httpcl::log().debug("{} Got HTTP status {}, {} bytes.", debugContext, res.status, res.content.size());

    // Parse loaded JSON
    if (res.status >= 200 && res.status < 300) {
        std::stringstream ss(res.content, std::ios_base::in);

        httpcl::log().debug("{} Parsing OpenAPI spec", debugContext);
        auto config = parseOpenAPIConfig(ss);
        // Add a default server and add missing server uri parts.
        if (config.servers.empty())
            config.servers.emplace_back();
        for (auto& server : config.servers) {
            if (server.scheme.empty())
                server.scheme = uriParts.scheme;
            if (server.host.empty()) {
                server.host = uriParts.host;
                server.port = uriParts.port;
            }
        }
        httpcl::log().debug("{} Parsed spec has {} methods.", debugContext, config.methodPath.size());

        return config;
    }

    throw httpcl::IHttpClient::Error(
        res,
        stx::format(
            "Error configuring OpenAPI service from URI: '{}', status: {}, content: '{}'",
            url,
            res.status,
            res.content));
}

}
