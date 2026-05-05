package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.yaml.snakeyaml.LoaderOptions;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.constructor.SafeConstructor;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URL;
import java.net.URLConnection;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

/**
 * Parser for OpenAPI 3.0 specifications, with full support for the zswag
 * extensions ({@code x-zserio-request-part}, {@code application/x-zserio-object}
 * request bodies, OAuth2 {@code clientCredentials} flow).
 *
 * <p>Mirrors the C++ {@code openapi-parser.cpp} dispatch model:
 * <ul>
 *   <li>Only HTTP methods GET/POST/PUT/DELETE are recognised; PATCH operations
 *       are ignored (see README "OpenAPI Options Interoperability" — patch
 *       cannot be realised over the zserio transport interface).</li>
 *   <li>Only the OAuth2 {@code clientCredentials} flow is accepted; other
 *       flows ({@code authorizationCode}, {@code implicit}, {@code password})
 *       cause the scheme to be rejected with {@link IllegalArgumentException}.</li>
 *   <li>Top-level {@code security:} is loaded as the default, applied to any
 *       operation that does not declare its own {@code security}.</li>
 *   <li>Per-operation {@code security} preserves the OR-of-AND structure as
 *       a list of {@link SecurityRequirement} alternatives. Empty list
 *       ({@code security: []}) means "explicitly no auth required".</li>
 * </ul>
 */
public class OpenAPIParser {
    private static final Logger logger = LoggerFactory.getLogger(OpenAPIParser.class);

    private final Map<String, Object> spec;
    private final Map<String, MethodInfo> methods = new LinkedHashMap<>();
    private final Map<String, SecurityScheme> securitySchemes = new LinkedHashMap<>();
    private final List<String> servers = new ArrayList<>();

    /** Top-level (default) security requirement; null = no global default. */
    @Nullable
    private List<SecurityRequirement> defaultSecurity;

    /**
     * Parses a spec from a URL/path. For HTTPS URLs, the caller may need to
     * supply OAuth2 tokens via the {@link #fetch(String, java.util.function.Consumer)}
     * helper; this constructor does not authenticate the spec fetch.
     */
    public OpenAPIParser(@NotNull String specLocation) throws IOException {
        this(loadSpec(specLocation, h -> {}));
    }

    /**
     * Parses a spec where the caller has already added auth headers (e.g.
     * an OAuth2 bearer token for {@code useForSpecFetch}) via
     * {@code headerInjector}.
     */
    public OpenAPIParser(@NotNull String specLocation,
                         @NotNull java.util.function.Consumer<URLConnection> headerInjector) throws IOException {
        this(loadSpec(specLocation, headerInjector));
    }

    private OpenAPIParser(@NotNull Map<String, Object> spec) {
        this.spec = spec;
        parseSpec();
    }

    @NotNull
    @SuppressWarnings("unchecked")
    private static Map<String, Object> loadSpec(@NotNull String location,
                                                @NotNull java.util.function.Consumer<URLConnection> headerInjector) throws IOException {
        logger.info("Loading OpenAPI spec from: {}", location);
        InputStream input;
        if (location.startsWith("http://") || location.startsWith("https://")) {
            URLConnection conn = new URL(location).openConnection();
            headerInjector.accept(conn);
            input = conn.getInputStream();
        } else {
            input = Files.newInputStream(Paths.get(location));
        }
        try (input) {
            LoaderOptions options = new LoaderOptions();
            options.setAllowDuplicateKeys(false);
            Yaml yaml = new Yaml(new SafeConstructor(options));
            Map<String, Object> loaded = yaml.load(input);
            if (loaded == null) {
                throw new IOException("Failed to load OpenAPI spec - empty or invalid YAML");
            }
            return loaded;
        }
    }

    @SuppressWarnings("unchecked")
    private void parseSpec() {
        // servers
        List<Map<String, Object>> serversList = (List<Map<String, Object>>) spec.get("servers");
        if (serversList != null) {
            for (Map<String, Object> server : serversList) {
                String url = (String) server.get("url");
                if (url != null) {
                    servers.add(url);
                    logger.debug("Found server: {}", url);
                }
            }
        }

        // securitySchemes
        Map<String, Object> components = (Map<String, Object>) spec.get("components");
        if (components != null) {
            Map<String, Object> securitySchemesMap = (Map<String, Object>) components.get("securitySchemes");
            if (securitySchemesMap != null) {
                parseSecuritySchemes(securitySchemesMap);
            }
        }

        // root-level (default) security
        Object rootSec = spec.get("security");
        if (rootSec instanceof List) {
            this.defaultSecurity = parseSecurityList((List<Map<String, Object>>) rootSec);
            logger.debug("Parsed root-level default security: {} alternatives", defaultSecurity.size());
        }

        // paths
        Map<String, Object> paths = (Map<String, Object>) spec.get("paths");
        if (paths != null) {
            parsePaths(paths);
        }
    }

    @SuppressWarnings("unchecked")
    private void parseSecuritySchemes(@NotNull Map<String, Object> schemesMap) {
        for (Map.Entry<String, Object> entry : schemesMap.entrySet()) {
            String name = entry.getKey();
            Map<String, Object> schemeData = (Map<String, Object>) entry.getValue();
            String typeStr = (String) schemeData.get("type");
            SecuritySchemeType type = parseSecuritySchemeType(typeStr);

            SecurityScheme.Builder builder = SecurityScheme.builder(name, type);

            switch (type) {
                case HTTP:
                    builder.scheme((String) schemeData.get("scheme"));
                    break;
                case API_KEY:
                    builder.apiKeyLocation(parseParameterLocation((String) schemeData.get("in")));
                    builder.apiKeyName((String) schemeData.get("name"));
                    break;
                case OAUTH2:
                    parseOAuth2Flows(name, (Map<String, Object>) schemeData.get("flows"), builder);
                    break;
                case OPEN_ID_CONNECT:
                    // Not supported by zswag clients; we still parse so the spec doesn't fail to load,
                    // but applySecurityScheme will refuse to dispatch a request that requires it.
                    logger.warn("Security scheme '{}' uses openIdConnect, which is not supported by zswag clients", name);
                    break;
            }

            SecurityScheme scheme = builder.build();
            securitySchemes.put(name, scheme);
            logger.debug("Parsed security scheme: {} ({})", name, type);
        }
    }

    @SuppressWarnings("unchecked")
    private void parseOAuth2Flows(@NotNull String schemeName, @Nullable Map<String, Object> flows,
                                   @NotNull SecurityScheme.Builder builder) {
        if (flows == null) {
            throw new IllegalArgumentException("OAuth2 scheme '" + schemeName + "' is missing the 'flows' object");
        }
        Map<String, Object> clientCredentials = (Map<String, Object>) flows.get("clientCredentials");
        if (clientCredentials == null) {
            // Match C++ openapi-parser.cpp:381 — only clientCredentials is supported.
            throw new IllegalArgumentException(
                    "OAuth2 scheme '" + schemeName + "': only the 'clientCredentials' flow is supported by zswag" +
                    " (got flows: " + flows.keySet() + ")");
        }
        builder.tokenUrl((String) clientCredentials.get("tokenUrl"));
        builder.refreshUrl((String) clientCredentials.get("refreshUrl"));
        Object scopes = clientCredentials.get("scopes");
        if (scopes instanceof Map) {
            for (Map.Entry<?, ?> e : ((Map<?, ?>) scopes).entrySet()) {
                builder.addOAuth2Scope(String.valueOf(e.getKey()), String.valueOf(e.getValue()));
            }
        }
    }

    @SuppressWarnings("unchecked")
    private void parsePaths(@NotNull Map<String, Object> paths) {
        for (Map.Entry<String, Object> pathEntry : paths.entrySet()) {
            String pathTemplate = pathEntry.getKey();
            Map<String, Object> pathItem = (Map<String, Object>) pathEntry.getValue();
            // PATCH is intentionally absent — see class javadoc.
            for (String httpMethod : Arrays.asList("get", "post", "put", "delete")) {
                Map<String, Object> operation = (Map<String, Object>) pathItem.get(httpMethod);
                if (operation != null) {
                    parseOperation(pathTemplate, httpMethod.toUpperCase(Locale.ROOT), operation);
                }
            }
            // Warn if patch is declared so users know it'll be silently ignored.
            if (pathItem.get("patch") != null) {
                logger.warn("Path '{}' declares a PATCH operation which zswag does not support; it will be ignored.", pathTemplate);
            }
        }
    }

    @SuppressWarnings("unchecked")
    private void parseOperation(@NotNull String pathTemplate, @NotNull String httpMethod,
                                @NotNull Map<String, Object> operation) {
        String operationId = (String) operation.get("operationId");
        if (operationId == null) {
            operationId = httpMethod + pathTemplate.replaceAll("[^a-zA-Z0-9]", "_");
        }

        MethodInfo methodInfo = new MethodInfo(operationId, pathTemplate, httpMethod);

        // parameters
        List<Map<String, Object>> parameters = (List<Map<String, Object>>) operation.get("parameters");
        if (parameters != null) {
            for (Map<String, Object> param : parameters) {
                methodInfo.addParameter(parseParameter(param, pathTemplate));
            }
        }

        // requestBody (body parameter is implicit when application/x-zserio-object content type is declared)
        Map<String, Object> requestBody = (Map<String, Object>) operation.get("requestBody");
        if (requestBody != null) {
            Map<String, Object> content = (Map<String, Object>) requestBody.get("content");
            if (content != null && content.containsKey("application/x-zserio-object")) {
                methodInfo.bodyRequestObject = true;
            } else if (content != null && !content.isEmpty()) {
                logger.warn("Operation {} {} has a requestBody with media types {} — only application/x-zserio-object is consumed by zswag",
                        httpMethod, pathTemplate, content.keySet());
            }
        }

        // security: per-op overrides global
        Object opSec = operation.get("security");
        if (opSec instanceof List) {
            methodInfo.security = parseSecurityList((List<Map<String, Object>>) opSec);
        } else {
            methodInfo.security = null; // inherit default
        }

        methods.put(operationId, methodInfo);
        logger.debug("Parsed operation: {} {} ({})", httpMethod, pathTemplate, operationId);
    }

    @SuppressWarnings("unchecked")
    @NotNull
    private static List<SecurityRequirement> parseSecurityList(@NotNull List<Map<String, Object>> raw) {
        List<SecurityRequirement> alternatives = new ArrayList<>();
        for (Map<String, Object> alt : raw) {
            Map<String, List<String>> required = new LinkedHashMap<>();
            for (Map.Entry<String, Object> e : alt.entrySet()) {
                List<String> scopes = new ArrayList<>();
                if (e.getValue() instanceof List) {
                    for (Object s : (List<?>) e.getValue()) {
                        scopes.add(String.valueOf(s));
                    }
                }
                required.put(e.getKey(), scopes);
            }
            alternatives.add(new SecurityRequirement(required));
        }
        return alternatives;
    }

    @SuppressWarnings("unchecked")
    @NotNull
    private OpenAPIParameter parseParameter(@NotNull Map<String, Object> paramData, @NotNull String pathTemplate) {
        String name = (String) paramData.get("name");
        ParameterLocation location = parseParameterLocation((String) paramData.get("in"));

        OpenAPIParameter.Builder builder = OpenAPIParameter.builder(name, location);

        Boolean required = (Boolean) paramData.get("required");
        if (required != null) builder.required(required);

        String style = (String) paramData.get("style");
        if (style != null) {
            ParameterStyle ps = parseParameterStyle(style);
            validateStyleLocation(name, location, ps, pathTemplate);
            builder.style(ps);
        }

        Boolean explode = (Boolean) paramData.get("explode");
        if (explode != null) builder.explode(explode);

        Map<String, Object> schema = (Map<String, Object>) paramData.get("schema");
        if (schema != null) {
            String format = (String) schema.get("format");
            if (format != null) builder.format(parseParameterFormat(format));
        }

        // The zswag extension that drives request decomposition.
        Object xrp = paramData.get("x-zserio-request-part");
        if (xrp != null) {
            builder.requestPart(String.valueOf(xrp));
        }

        return builder.build();
    }

    private static void validateStyleLocation(@NotNull String paramName, @NotNull ParameterLocation loc,
                                              @NotNull ParameterStyle style, @NotNull String pathTemplate) {
        // Mirrors C++ openapi-parser.cpp:191-209
        switch (style) {
            case MATRIX:
            case LABEL:
                if (loc != ParameterLocation.PATH) {
                    throw new IllegalArgumentException(
                            "Parameter '" + paramName + "' on " + pathTemplate +
                            ": style '" + style + "' is only valid for path parameters");
                }
                break;
            case FORM:
                if (loc != ParameterLocation.QUERY && loc != ParameterLocation.COOKIE) {
                    throw new IllegalArgumentException(
                            "Parameter '" + paramName + "' on " + pathTemplate +
                            ": style 'form' is only valid for query or cookie parameters");
                }
                break;
            case SIMPLE:
                if (loc == ParameterLocation.QUERY || loc == ParameterLocation.COOKIE) {
                    throw new IllegalArgumentException(
                            "Parameter '" + paramName + "' on " + pathTemplate +
                            ": style 'simple' is not valid for query or cookie parameters");
                }
                break;
            default:
                break;
        }
    }

    @NotNull
    private SecuritySchemeType parseSecuritySchemeType(@Nullable String type) {
        if (type == null) return SecuritySchemeType.HTTP;
        switch (type.toLowerCase(Locale.ROOT)) {
            case "http": return SecuritySchemeType.HTTP;
            case "apikey": return SecuritySchemeType.API_KEY;
            case "oauth2": return SecuritySchemeType.OAUTH2;
            case "openidconnect": return SecuritySchemeType.OPEN_ID_CONNECT;
            default:
                throw new IllegalArgumentException("Unknown security scheme type: " + type);
        }
    }

    @NotNull
    private ParameterLocation parseParameterLocation(@Nullable String location) {
        if (location == null) return ParameterLocation.QUERY;
        switch (location.toLowerCase(Locale.ROOT)) {
            case "path": return ParameterLocation.PATH;
            case "query": return ParameterLocation.QUERY;
            case "header": return ParameterLocation.HEADER;
            case "cookie": return ParameterLocation.COOKIE;
            default:
                throw new IllegalArgumentException("Unknown parameter location: " + location);
        }
    }

    @NotNull
    private ParameterStyle parseParameterStyle(@Nullable String style) {
        if (style == null) return ParameterStyle.SIMPLE;
        switch (style.toLowerCase(Locale.ROOT)) {
            case "simple": return ParameterStyle.SIMPLE;
            case "label": return ParameterStyle.LABEL;
            case "matrix": return ParameterStyle.MATRIX;
            case "form": return ParameterStyle.FORM;
            case "spacedelimited": return ParameterStyle.SPACE_DELIMITED;
            case "pipedelimited": return ParameterStyle.PIPE_DELIMITED;
            case "deepobject": return ParameterStyle.DEEP_OBJECT;
            default:
                throw new IllegalArgumentException("Unknown parameter style: " + style);
        }
    }

    @NotNull
    private ParameterFormat parseParameterFormat(@Nullable String format) {
        if (format == null) return ParameterFormat.STRING;
        switch (format.toLowerCase(Locale.ROOT)) {
            case "hex": return ParameterFormat.HEX;
            case "byte":            // Alias for base64 per OpenAPI spec
            case "base64": return ParameterFormat.BASE64;
            case "base64url": return ParameterFormat.BASE64URL;
            case "binary": return ParameterFormat.BINARY;
            case "string": return ParameterFormat.STRING;
            default:
                logger.debug("Unknown parameter format '{}', defaulting to STRING", format);
                return ParameterFormat.STRING;
        }
    }

    @NotNull public List<String> getServers() { return Collections.unmodifiableList(servers); }
    @NotNull public Map<String, SecurityScheme> getSecuritySchemes() { return Collections.unmodifiableMap(securitySchemes); }
    @Nullable public MethodInfo getMethod(@NotNull String operationId) { return methods.get(operationId); }
    @NotNull public Map<String, MethodInfo> getMethods() { return Collections.unmodifiableMap(methods); }

    /** Top-level default security requirement (or empty if no root-level security). */
    @NotNull public Optional<List<SecurityRequirement>> getDefaultSecurity() {
        return Optional.ofNullable(defaultSecurity == null ? null : Collections.unmodifiableList(defaultSecurity));
    }

    /** One OpenAPI operation. */
    public static class MethodInfo {
        private final String operationId;
        private final String pathTemplate;
        private final String httpMethod;
        private final List<OpenAPIParameter> parameters = new ArrayList<>();
        boolean bodyRequestObject;
        @Nullable List<SecurityRequirement> security;  // null = inherit global default

        public MethodInfo(@NotNull String operationId, @NotNull String pathTemplate, @NotNull String httpMethod) {
            this.operationId = operationId;
            this.pathTemplate = pathTemplate;
            this.httpMethod = httpMethod;
        }

        public void addParameter(@NotNull OpenAPIParameter parameter) { parameters.add(parameter); }

        @NotNull public String getOperationId() { return operationId; }
        @NotNull public String getPathTemplate() { return pathTemplate; }
        @NotNull public String getHttpMethod() { return httpMethod; }
        @NotNull public List<OpenAPIParameter> getParameters() { return Collections.unmodifiableList(parameters); }

        /** True if the operation declares an {@code application/x-zserio-object} request body. */
        public boolean hasZserioBody() { return bodyRequestObject; }

        /**
         * Per-operation security as an OR-of-AND list of alternatives, or empty
         * if the operation should fall back to the global default security.
         * An empty list (operation explicitly declares {@code security: []})
         * means "no auth required".
         */
        @NotNull
        public Optional<List<SecurityRequirement>> getSecurity() {
            return Optional.ofNullable(security == null ? null : Collections.unmodifiableList(security));
        }
    }
}
