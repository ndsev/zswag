#include "openapi-parser.hpp"

#include "httpcl/http-settings.hpp"
#include "httpcl/uri.hpp"

#include "yaml-cpp/yaml.h"
#include "stx/format.h"
#include <httplib.h>

#include <sstream>
#include <string>

using namespace std::string_literals;

namespace {

struct Scope {
    std::string name_;
    Scope const* parent_ = nullptr;
    YAML::Node node_;

    explicit Scope(std::string name, YAML::Node const& n, Scope const* parent = nullptr)
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
        return std::runtime_error(stx::format(
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
        return std::runtime_error(stx::format(
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
        return std::runtime_error(stx::format(
            "ERROR while parsing OpenAPI schema:\n"
            "    At {}:\n"
            "        Mandatory field `{}` is missing.\n",
            str(),
            field));
    }

    operator bool() const {
        return node_.operator bool();
    }

    Scope operator[] (char const* name) const {
        auto child = node_[name];
        return Scope(name, child, this);
    }

    Scope operator[] (std::string const& name) const {
        return operator[](name.c_str());
    }

    Scope mandatoryChild(std::string const& name) const {
        auto result = operator[](name);
        if (!result)
            throw missingFieldError(name);
        return result;
    }

    template<typename T>
    T as() const {
        return node_.as<T>();
    }

    void forEach(std::function<void(Scope const& child)> const& fun) {
        if (!node_ || !fun || !(node_.IsMap() || node_.IsSequence()))
            return;
        size_t i = 0;
        for (auto const& child : node_) {
            if (node_.IsMap())
                fun(Scope(child.first.as<std::string>(), child.second, this));
            else
                fun(Scope(stx::to_string(i), child, this));
            ++i;
        }
    }
};

}

namespace zswagcl
{

static auto parseParameterLocation(Scope const& inNode)
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
static auto parseParameterSchema(Scope const& schemaNode)
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
static void parseParameterStyle(Scope const& styleNode,
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

static void parseParameterExplode(Scope const& explodeNode,
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

static bool parseMethodParameter(Scope const& parameterNode,
                                 OpenAPIConfig::Path& path)
{
    auto nameNode = parameterNode.mandatoryChild("name");
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

static void parseMethodBody(Scope const& methodNode,
                            OpenAPIConfig::Path& path)
{
    if (auto bodyNode = methodNode["requestBody"]) {
        if (auto contentNode = bodyNode["content"]) {
            for (auto contentTypeNode : contentNode.node_) {
                auto contentType = contentTypeNode.first.as<std::string>();
                if (contentType != ZSERIO_OBJECT_CONTENT_TYPE)
                    throw contentNode.valueError(contentType, {ZSERIO_OBJECT_CONTENT_TYPE});
                path.bodyRequestObject = true;
            }
        }
    }
}

static OpenAPIConfig::SecurityAlternatives parseSecurity(
        Scope const& securityNode,
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
                        const Scope& pathNode,
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
    const Scope& schemeNode,
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

static void parsePath(const Scope& pathNode,
                      OpenAPIConfig& config)
{
    static const char* supportedMethods[] = {
        "get", "post", "put", "delete"
    };

    for (const auto method : supportedMethods) {
        parseMethod(method, pathNode, config);
    }
}

static void parseServer(const Scope& serverNode,
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
    Scope docScope{"", doc};
    docScope["servers"].forEach([&](auto const& serverNode){
        try { parseServer(serverNode, config); }
        catch (const httpcl::URIError& e) {
            throw std::runtime_error(
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
