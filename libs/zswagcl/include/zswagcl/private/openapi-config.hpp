#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>

#include "zswagcl/export.hpp"
#include "httpcl/uri.hpp"
#include "httpcl/http-settings.hpp"

namespace zswagcl
{

struct OpenAPIConfig
{
    enum class ParameterLocation {
        Path,
        Query,
        Header
    };

    enum class SecuritySchemeType {
        HttpBasic,
        HttpBearer,
        ApiKeyQuery,
        ApiKeyHeader,
        ApiKeyCookie,
        OAuth2ClientCredentials
    };

    struct SecurityScheme
    {
        SecuritySchemeType type;
        std::string oauthTokenUrl; // For oauth2
        std::string oauthRefreshUrl; // For oauth2
        std::map<std::string, std::string> oauthScopes; // Scope -> Description
        std::string apiKeyName; // Header/Query/Cookie name
        std::string id; // Scheme name for introspection/debugging
    };
    using SecuritySchemePtr = std::shared_ptr<SecurityScheme>;

    /**
     * Security Scheme References by operations.
     *
     * Disjunctive normal form ([A [AND B]+][ OR C [AND D]+]+) of required
     * security schemes. An empty vector is used to encode that no auth
     * scheme is required.
     */
    struct SecurityRequirement
    {
        SecuritySchemePtr scheme;
        // For each operation, a subset of scopes may be defined which are required for it.
        std::vector<std::string> scopes;
    };
    using SecurityAlternative = std::vector<SecurityRequirement>;
    using SecurityAlternatives = std::vector<SecurityAlternative>;

    /**
     * Supported Authentication Schemes
     */

    struct Parameter {
        ParameterLocation location = ParameterLocation::Query;

        /**
         * Parameter identifier.
         */
        std::string ident;

        /**
         * Zserio structure field or function identifier.
         * The special identifier '*' represents the binary
         * encoded request object.
         */
        std::string field;

        /**
         * Default parameter value.
         * Used if value could not be read.
         */
        std::string defaultValue;

        /**
         * Parameter encoding format.
         */
        enum Format {
            /**
             * Default encoding.
             */
            String,

            /**
             * Hexadicamal (hexpair per octet) encoding.
             * Does not include a prefix.
             */
            Hex,

            /**
             * Base64 encoding.
             */
            Base64,    // Standard
            Base64url, // URL safe

            /**
             * Binary (octet) encoding
             */
            Binary,
        } format = String;

        /**
         * Parameter style.
         *
         * https://tools.ietf.org/html/rfc6570#section-3.2.7
         */
        enum Style {
            /**
             * Simple style parameter defined by RFC 6570.
             * Template: {X}
             */
            Simple,

            /**
             * Label style parameter defined by RFC 6570.
             * Template: {.X}
             */
            Label,

            /**
             * Form style parameter defined by RFC 6570.
             * Template: {?X}
             */
            Form,

            /**
             * Path style parameter defined by RFC 6570.
             * Template {;X}
             */
            Matrix,
        } style = Simple;

        /**
         * If true, generate separate parameters for each array-value
         * or object field-value.
         */
        bool explode = false;
    };

    struct Path {
        /**
         * URI suffix.
         */
        std::string path;

        /**
         * HTTP method.
         */
        std::string httpMethod = "POST";

        /**
         * Parameter name to configuration.
         */
        std::map<std::string, Parameter> parameters;

        /**
         * Zserio structure field or function identifier that is transferred
         * as request body.
         * The special identifier '*' represents the binary
         * encoded request object.
         *
         * Ignored if HTTP-Method is GET.
         */
        bool bodyRequestObject = false;

        /**
         * Optional security schemes override for the global default.
         */
        std::optional<SecurityAlternatives> security;
    };

    /**
     * URI parts.
     */
    std::vector<httpcl::URIComponents> servers;

    /**
     * Map from service method name to path configuration.
     */
    std::map<std::string, Path> methodPath;

    /**
     * Available security schemes.
     */
    std::map<std::string, SecuritySchemePtr> securitySchemes;

    /**
     * Default security scheme for all paths. The default
     * is an empty array of combinations, which means no auth required.
     */
    SecurityAlternatives defaultSecurityScheme;

    /**
     * Original OpenAPI YAML string from which this config was parsed.
     */
    std::string content;
};

ZSWAGCL_EXPORT extern const std::string ZSERIO_OBJECT_CONTENT_TYPE;
ZSWAGCL_EXPORT extern const std::string ZSERIO_REQUEST_PART;
ZSWAGCL_EXPORT extern const std::string ZSERIO_REQUEST_PART_WHOLE;

}
