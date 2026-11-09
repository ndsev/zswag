#pragma once

#include <array>
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

/** Owned zswag transport metadata, with shared security-scheme references between requirements. */
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

    /**
     * Mapping between OpenAPI spec strings and SecuritySchemeType enum.
     * Single source of truth for both parsing and reverse lookup.
     */
    struct SecuritySchemeMapping {
        const char* openapiType;      // "http", "apiKey", "oauth2"
        const char* openapiSubtype;   // "basic"/"bearer", "query"/"header"/"cookie", "clientCredentials"
        SecuritySchemeType enumValue;
        const char* displayString;    // "http/basic", "apiKey/query", etc.
    };

    /**
     * Security scheme mapping table - single source of truth.
     * When adding new security scheme types, update this array and the size.
     * Using std::array with explicit size for reliable cross-platform constexpr behavior.
     */
    static constexpr std::array<SecuritySchemeMapping, 6> SECURITY_SCHEME_MAPPINGS = {{
        {"http", "basic", SecuritySchemeType::HttpBasic, "http/basic"},
        {"http", "bearer", SecuritySchemeType::HttpBearer, "http/bearer"},
        {"apiKey", "query", SecuritySchemeType::ApiKeyQuery, "apiKey/query"},
        {"apiKey", "header", SecuritySchemeType::ApiKeyHeader, "apiKey/header"},
        {"apiKey", "cookie", SecuritySchemeType::ApiKeyCookie, "apiKey/cookie"},
        {"oauth2", "clientCredentials", SecuritySchemeType::OAuth2ClientCredentials, "oauth2/clientCredentials"},
    }};

    /**
     * Bidirectional maps for security scheme lookups.
     * Auto-generated from SECURITY_SCHEME_MAPPINGS at static initialization.
     */
    struct SecuritySchemeMaps {
        // Forward: (type, subtype) -> enum
        std::map<std::pair<std::string, std::string>, SecuritySchemeType> forward;
        // Reverse: enum -> display string
        std::map<SecuritySchemeType, std::string> reverse;

        SecuritySchemeMaps() {
            for (const auto& m : SECURITY_SCHEME_MAPPINGS) {
                forward[{m.openapiType, m.openapiSubtype}] = m.enumValue;
                reverse[m.enumValue] = m.displayString;
            }
        }

        static const SecuritySchemeMaps& instance() {
            static SecuritySchemeMaps maps;
            return maps;
        }
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

    /** One mapped request field; parameters without x-zserio-request-part are not retained. */
    struct Parameter
    {
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
         * Fallback used by parameter formatting and Python request assembly.
         * The native parser currently leaves this empty; it does not read schema.default.
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
             * Hexadecimal without a prefix: integers use numeric digits (with
             * an optional minus), while byte/string buffers use hex pairs per octet.
             */
            Hex,

            /**
             * Base64 encoding.
             */
            Base64,     // Standard
            Base64url,  // URL safe

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
         * Uppercase HTTP method, such as GET or POST.
         */
        std::string httpMethod = "POST";

        /**
         * Parameter name to configuration.
         */
        std::map<std::string, Parameter> parameters;

        /**
         * True when requestBody advertises the zserio object content type.
         * The entire serialized request becomes the body; this is not a field
         * name. Native GET dispatch ignores this flag.
         */
        bool bodyRequestObject = false;

        /**
         * Absent inherits global security; an engaged empty list disables it.
         */
        std::optional<SecurityAlternatives> security;
    };

    /**
     * Advertised server URIs. Stream parsing may leave relative entries or an
     * empty list; fetchOpenAPIConfig resolves them against the fetched URL.
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

/**
 * Convert SecuritySchemeType enum to display string.
 * Returns "unknown" if type is not found.
 */
inline const char* securitySchemeTypeToString(OpenAPIConfig::SecuritySchemeType type) {
    const auto& maps = OpenAPIConfig::SecuritySchemeMaps::instance();
    auto it = maps.reverse.find(type);
    if (it != maps.reverse.end()) {
        return it->second.c_str();
    }
    return "unknown";
}

ZSWAGCL_EXPORT extern const std::string ZSERIO_OBJECT_CONTENT_TYPE;
ZSWAGCL_EXPORT extern const std::string ZSERIO_REQUEST_PART;
ZSWAGCL_EXPORT extern const std::string ZSERIO_REQUEST_PART_WHOLE;

}
