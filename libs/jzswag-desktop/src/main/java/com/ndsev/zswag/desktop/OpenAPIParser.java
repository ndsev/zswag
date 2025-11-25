package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.yaml.snakeyaml.Yaml;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

/**
 * Parser for OpenAPI 3.0 specifications.
 * Extracts paths, parameters, security schemes, and server URLs from OpenAPI specs.
 */
public class OpenAPIParser {
    private static final Logger logger = LoggerFactory.getLogger(OpenAPIParser.class);

    private final Map<String, Object> spec;
    private final Map<String, MethodInfo> methods = new HashMap<>();
    private final Map<String, SecurityScheme> securitySchemes = new HashMap<>();
    private final List<String> servers = new ArrayList<>();

    public OpenAPIParser(@NotNull String specLocation) throws IOException {
        this.spec = loadSpec(specLocation);
        parseSpec();
    }

    /**
     * Loads an OpenAPI spec from a file path or URL.
     */
    @NotNull
    @SuppressWarnings("unchecked")
    private Map<String, Object> loadSpec(@NotNull String location) throws IOException {
        logger.info("Loading OpenAPI spec from: {}", location);

        InputStream input;
        if (location.startsWith("http://") || location.startsWith("https://")) {
            input = new URL(location).openStream();
        } else {
            input = Files.newInputStream(Paths.get(location));
        }

        try (input) {
            Yaml yaml = new Yaml();
            Map<String, Object> loaded = yaml.load(input);
            if (loaded == null) {
                throw new IOException("Failed to load OpenAPI spec - empty or invalid YAML");
            }
            return loaded;
        }
    }

    /**
     * Parses the OpenAPI specification.
     */
    @SuppressWarnings("unchecked")
    private void parseSpec() {
        // Parse servers
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

        // Parse security schemes
        Map<String, Object> components = (Map<String, Object>) spec.get("components");
        if (components != null) {
            Map<String, Object> securitySchemesMap = (Map<String, Object>) components.get("securitySchemes");
            if (securitySchemesMap != null) {
                parseSecuritySchemes(securitySchemesMap);
            }
        }

        // Parse paths
        Map<String, Object> paths = (Map<String, Object>) spec.get("paths");
        if (paths != null) {
            parsePaths(paths);
        }
    }

    /**
     * Parses security schemes from the components section.
     */
    @SuppressWarnings("unchecked")
    private void parseSecuritySchemes(@NotNull Map<String, Object> schemesMap) {
        for (Map.Entry<String, Object> entry : schemesMap.entrySet()) {
            String name = entry.getKey();
            Map<String, Object> schemeData = (Map<String, Object>) entry.getValue();

            String typeStr = (String) schemeData.get("type");
            SecuritySchemeType type = parseSecuritySchemeType(typeStr);

            SecurityScheme.Builder builder = SecurityScheme.builder(name, type);

            if (type == SecuritySchemeType.HTTP) {
                String scheme = (String) schemeData.get("scheme");
                builder.scheme(scheme);
            } else if (type == SecuritySchemeType.API_KEY) {
                String inStr = (String) schemeData.get("in");
                String keyName = (String) schemeData.get("name");
                ParameterLocation location = parseParameterLocation(inStr);
                builder.apiKeyLocation(location);
                builder.apiKeyName(keyName);
            }

            SecurityScheme scheme = builder.build();
            securitySchemes.put(name, scheme);
            logger.debug("Parsed security scheme: {} ({})", name, type);
        }
    }

    /**
     * Parses paths and their operations.
     */
    @SuppressWarnings("unchecked")
    private void parsePaths(@NotNull Map<String, Object> paths) {
        for (Map.Entry<String, Object> pathEntry : paths.entrySet()) {
            String pathTemplate = pathEntry.getKey();
            Map<String, Object> pathItem = (Map<String, Object>) pathEntry.getValue();

            // Parse each HTTP method for this path
            for (String httpMethod : Arrays.asList("get", "post", "put", "delete", "patch")) {
                Map<String, Object> operation = (Map<String, Object>) pathItem.get(httpMethod);
                if (operation != null) {
                    parseOperation(pathTemplate, httpMethod.toUpperCase(), operation);
                }
            }
        }
    }

    /**
     * Parses an operation (HTTP method on a path).
     */
    @SuppressWarnings("unchecked")
    private void parseOperation(@NotNull String pathTemplate, @NotNull String httpMethod,
                                 @NotNull Map<String, Object> operation) {
        String operationId = (String) operation.get("operationId");
        if (operationId == null) {
            operationId = httpMethod + pathTemplate.replaceAll("[^a-zA-Z0-9]", "_");
        }

        MethodInfo methodInfo = new MethodInfo(pathTemplate, httpMethod);

        // Parse parameters
        List<Map<String, Object>> parameters = (List<Map<String, Object>>) operation.get("parameters");
        if (parameters != null) {
            for (Map<String, Object> param : parameters) {
                OpenAPIParameter parameter = parseParameter(param);
                methodInfo.addParameter(parameter);
            }
        }

        // Parse security requirements
        List<Map<String, Object>> security = (List<Map<String, Object>>) operation.get("security");
        if (security != null) {
            for (Map<String, Object> requirement : security) {
                methodInfo.securityRequirements.addAll(requirement.keySet());
            }
        }

        methods.put(operationId, methodInfo);
        logger.debug("Parsed operation: {} {} ({})", httpMethod, pathTemplate, operationId);
    }

    /**
     * Parses a parameter definition.
     */
    @SuppressWarnings("unchecked")
    @NotNull
    private OpenAPIParameter parseParameter(@NotNull Map<String, Object> paramData) {
        String name = (String) paramData.get("name");
        String inStr = (String) paramData.get("in");
        ParameterLocation location = parseParameterLocation(inStr);

        OpenAPIParameter.Builder builder = OpenAPIParameter.builder(name, location);

        // Parse required
        Boolean required = (Boolean) paramData.get("required");
        if (required != null) {
            builder.required(required);
        }

        // Parse style
        String style = (String) paramData.get("style");
        if (style != null) {
            builder.style(parseParameterStyle(style));
        }

        // Parse explode
        Boolean explode = (Boolean) paramData.get("explode");
        if (explode != null) {
            builder.explode(explode);
        }

        // Parse schema for format hints
        Map<String, Object> schema = (Map<String, Object>) paramData.get("schema");
        if (schema != null) {
            String format = (String) schema.get("format");
            if (format != null) {
                builder.format(parseParameterFormat(format));
            }
        }

        return builder.build();
    }

    @NotNull
    private SecuritySchemeType parseSecuritySchemeType(@Nullable String type) {
        if (type == null) return SecuritySchemeType.HTTP;
        switch (type.toLowerCase()) {
            case "http": return SecuritySchemeType.HTTP;
            case "apikey": return SecuritySchemeType.API_KEY;
            case "oauth2": return SecuritySchemeType.OAUTH2;
            case "openidconnect": return SecuritySchemeType.OPEN_ID_CONNECT;
            default:
                logger.warn("Unknown security scheme type: {}, defaulting to HTTP", type);
                return SecuritySchemeType.HTTP;
        }
    }

    @NotNull
    private ParameterLocation parseParameterLocation(@Nullable String location) {
        if (location == null) return ParameterLocation.QUERY;
        switch (location.toLowerCase()) {
            case "path": return ParameterLocation.PATH;
            case "query": return ParameterLocation.QUERY;
            case "header": return ParameterLocation.HEADER;
            case "cookie": return ParameterLocation.COOKIE;
            default:
                logger.warn("Unknown parameter location: {}, defaulting to QUERY", location);
                return ParameterLocation.QUERY;
        }
    }

    @NotNull
    private ParameterStyle parseParameterStyle(@Nullable String style) {
        if (style == null) return ParameterStyle.SIMPLE;
        switch (style.toLowerCase()) {
            case "simple": return ParameterStyle.SIMPLE;
            case "label": return ParameterStyle.LABEL;
            case "matrix": return ParameterStyle.MATRIX;
            case "form": return ParameterStyle.FORM;
            case "spacedelimited": return ParameterStyle.SPACE_DELIMITED;
            case "pipedelimited": return ParameterStyle.PIPE_DELIMITED;
            case "deepobject": return ParameterStyle.DEEP_OBJECT;
            default:
                logger.warn("Unknown parameter style: {}, defaulting to SIMPLE", style);
                return ParameterStyle.SIMPLE;
        }
    }

    @NotNull
    private ParameterFormat parseParameterFormat(@Nullable String format) {
        if (format == null) return ParameterFormat.STRING;
        switch (format.toLowerCase()) {
            case "hex": return ParameterFormat.HEX;
            case "base64": return ParameterFormat.BASE64;
            case "base64url": return ParameterFormat.BASE64URL;
            case "binary": return ParameterFormat.BINARY;
            default:
                return ParameterFormat.STRING;
        }
    }

    @NotNull
    public List<String> getServers() {
        return Collections.unmodifiableList(servers);
    }

    @NotNull
    public Map<String, SecurityScheme> getSecuritySchemes() {
        return Collections.unmodifiableMap(securitySchemes);
    }

    @Nullable
    public MethodInfo getMethod(@NotNull String operationId) {
        return methods.get(operationId);
    }

    /**
     * Information about an OpenAPI operation.
     */
    public static class MethodInfo {
        private final String pathTemplate;
        private final String httpMethod;
        private final List<OpenAPIParameter> parameters = new ArrayList<>();
        private final Set<String> securityRequirements = new HashSet<>();

        public MethodInfo(@NotNull String pathTemplate, @NotNull String httpMethod) {
            this.pathTemplate = pathTemplate;
            this.httpMethod = httpMethod;
        }

        public void addParameter(@NotNull OpenAPIParameter parameter) {
            parameters.add(parameter);
        }

        @NotNull
        public String getPathTemplate() {
            return pathTemplate;
        }

        @NotNull
        public String getHttpMethod() {
            return httpMethod;
        }

        @NotNull
        public List<OpenAPIParameter> getParameters() {
            return Collections.unmodifiableList(parameters);
        }

        @NotNull
        public Set<String> getSecurityRequirements() {
            return Collections.unmodifiableSet(securityRequirements);
        }
    }
}
