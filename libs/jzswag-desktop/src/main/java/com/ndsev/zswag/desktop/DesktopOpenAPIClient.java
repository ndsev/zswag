package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.*;

/**
 * Desktop implementation of OpenAPI client.
 * Handles OpenAPI method calls, parameter encoding, and security.
 */
public class DesktopOpenAPIClient implements IOpenAPIClient {
    private static final Logger logger = LoggerFactory.getLogger(DesktopOpenAPIClient.class);

    private final String specLocation;
    private final IHttpClient httpClient;
    private final OpenAPIParser parser;
    private final String baseUrl;

    public DesktopOpenAPIClient(@NotNull String specLocation, @NotNull IHttpClient httpClient) throws IOException {
        this.specLocation = specLocation;
        this.httpClient = httpClient;
        this.parser = new OpenAPIParser(specLocation);

        // Determine base URL from servers
        List<String> servers = parser.getServers();
        String serverUrl = !servers.isEmpty() ? servers.get(0) : "";

        // If server URL is relative (empty or starts with /) and spec location is a URL,
        // extract base URL from spec location
        String resolvedBaseUrl;
        boolean isRelativeUrl = serverUrl.isEmpty() || serverUrl.startsWith("/");

        if (isRelativeUrl && specLocation.startsWith("http")) {
            try {
                java.net.URL url = new java.net.URL(specLocation);
                String protocol = url.getProtocol();
                String host = url.getHost();
                int port = url.getPort();
                String basePath = serverUrl.isEmpty() ? "" : serverUrl;

                if (port != -1) {
                    resolvedBaseUrl = protocol + "://" + host + ":" + port + basePath;
                } else {
                    resolvedBaseUrl = protocol + "://" + host + basePath;
                }
                logger.info("Resolved relative server URL '{}' to: {}", serverUrl, resolvedBaseUrl);
            } catch (java.net.MalformedURLException e) {
                resolvedBaseUrl = serverUrl;
                logger.warn("Failed to parse spec location URL: {}", e.getMessage());
            }
        } else if (!serverUrl.isEmpty()) {
            resolvedBaseUrl = serverUrl;
            logger.info("Using absolute server URL: {}", resolvedBaseUrl);
        } else {
            // No server URL and spec is not from HTTP - use empty
            resolvedBaseUrl = "";
            logger.warn("No servers defined in OpenAPI spec and cannot infer from spec location");
        }

        this.baseUrl = resolvedBaseUrl;
    }

    @Override
    @Nullable
    public byte[] callMethod(@NotNull String methodPath, @NotNull Map<String, Object> parameters,
                             @Nullable byte[] requestBody) throws HttpException {
        // Find the method info
        OpenAPIParser.MethodInfo methodInfo = findMethodInfo(methodPath, parameters);
        if (methodInfo == null) {
            throw new HttpException("Method not found in OpenAPI spec: " + methodPath);
        }

        logger.debug("Calling method: {} {}", methodInfo.getHttpMethod(), methodPath);

        // Build the request URL
        String url = buildRequestUrl(methodInfo, parameters);

        // Build request headers
        Map<String, String> headers = new HashMap<>();
        addParametersToHeaders(methodInfo, parameters, headers);
        addSecurityHeaders(methodInfo, headers);

        // Build the HTTP request
        com.ndsev.zswag.api.HttpRequest.Builder requestBuilder = com.ndsev.zswag.api.HttpRequest.builder()
                .method(methodInfo.getHttpMethod())
                .url(url)
                .headers(headers);

        // Add request body if present
        if (requestBody != null) {
            requestBuilder.body(requestBody);
            // Set content-type for binary zserio data
            if (!headers.containsKey("Content-Type")) {
                requestBuilder.header("Content-Type", "application/octet-stream");
            }
        }

        // Execute the request
        com.ndsev.zswag.api.HttpResponse response = httpClient.execute(requestBuilder.build());

        // Check for success
        if (!response.isSuccessful()) {
            String errorMsg = String.format("HTTP %d: %s", response.getStatusCode(), response.getStatusMessage());
            throw new HttpException(errorMsg, response.getStatusCode(), response.getBody());
        }

        return response.getBody();
    }

    /**
     * Finds method info by operation ID or path template.
     */
    @Nullable
    private OpenAPIParser.MethodInfo findMethodInfo(@NotNull String methodPath, @NotNull Map<String, Object> parameters) {
        // Try direct operation ID lookup first (e.g., "power", "intSum")
        OpenAPIParser.MethodInfo info = parser.getMethod(methodPath);
        if (info != null) {
            return info;
        }

        // Try with HTTP method prefix (e.g., "GETpower", "POST/path")
        for (String possibleMethod : Arrays.asList("GET" + methodPath, "POST" + methodPath,
                                                     "PUT" + methodPath, "DELETE" + methodPath, "PATCH" + methodPath)) {
            info = parser.getMethod(possibleMethod);
            if (info != null) {
                return info;
            }
        }

        // If not found, we could implement more sophisticated path template matching here
        return null;
    }

    /**
     * Builds the full request URL with path and query parameters.
     */
    @NotNull
    private String buildRequestUrl(@NotNull OpenAPIParser.MethodInfo methodInfo, @NotNull Map<String, Object> parameters) {
        String path = methodInfo.getPathTemplate();

        // Substitute path parameters
        Map<String, String> queryParams = new HashMap<>();
        for (OpenAPIParameter param : methodInfo.getParameters()) {
            Object value = parameters.get(param.getName());
            if (value == null) {
                if (param.isRequired()) {
                    logger.warn("Required parameter missing: {}", param.getName());
                }
                continue;
            }

            String encoded = ParameterEncoder.encodeParameter(param, value);

            if (param.getLocation() == ParameterLocation.PATH) {
                // Replace path parameter
                path = path.replace("{" + param.getName() + "}", encoded);
            } else if (param.getLocation() == ParameterLocation.QUERY) {
                // Add to query parameters
                queryParams.put(param.getName(), encoded);
            }
        }

        // Build full URL
        StringBuilder url = new StringBuilder(baseUrl);
        if (!baseUrl.isEmpty() && !baseUrl.endsWith("/") && !path.startsWith("/")) {
            url.append("/");
        }
        url.append(path);

        // Add query string
        if (!queryParams.isEmpty()) {
            String queryString = ParameterEncoder.buildQueryString(queryParams);
            url.append("?").append(queryString);
        }

        // Add query parameters from settings
        Map<String, String> settingsQueryParams = httpClient.getSettings().getQueryParameters();
        if (!settingsQueryParams.isEmpty()) {
            String settingsQuery = ParameterEncoder.buildQueryString(settingsQueryParams);
            url.append(queryParams.isEmpty() ? "?" : "&").append(settingsQuery);
        }

        return url.toString();
    }

    /**
     * Adds header parameters to the request.
     * Note: Generic headers from HttpSettings are added by DesktopHttpClient, not here.
     * This method only processes operation-specific header parameters from the parameters map.
     */
    private void addParametersToHeaders(@NotNull OpenAPIParser.MethodInfo methodInfo,
                                         @NotNull Map<String, Object> parameters,
                                         @NotNull Map<String, String> headers) {
        // Process operation-specific header parameters only
        // Generic headers from HttpSettings are added by DesktopHttpClient.execute()
        for (OpenAPIParameter param : methodInfo.getParameters()) {
            if (param.getLocation() == ParameterLocation.HEADER) {
                Object value = parameters.get(param.getName());
                if (value != null) {
                    String encoded = ParameterEncoder.encodeParameter(param, value);
                    headers.put(param.getName(), encoded);
                }
            }
        }
    }

    /**
     * Adds security-related headers based on the method's security requirements.
     */
    private void addSecurityHeaders(@NotNull OpenAPIParser.MethodInfo methodInfo, @NotNull Map<String, String> headers) {
        Set<String> requirements = methodInfo.getSecurityRequirements();
        Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();

        for (String requirement : requirements) {
            SecurityScheme scheme = schemes.get(requirement);
            if (scheme == null) {
                logger.warn("Security scheme not found: {}", requirement);
                continue;
            }

            applySecurityScheme(scheme, headers);
        }
    }

    /**
     * Applies a security scheme to the request.
     */
    private void applySecurityScheme(@NotNull SecurityScheme scheme, @NotNull Map<String, String> headers) {
        HttpSettings settings = httpClient.getSettings();

        switch (scheme.getType()) {
            case HTTP:
                // Basic and Bearer auth are handled by HttpClient
                break;

            case API_KEY:
                if (scheme.getApiKeyLocation() == ParameterLocation.HEADER) {
                    String keyName = scheme.getApiKeyName();
                    String keyValue = settings.getApiKeys().get(keyName);
                    if (keyValue != null) {
                        headers.put(keyName, keyValue);
                    }
                }
                // Query and cookie API keys would be handled elsewhere
                break;

            case OAUTH2:
                // OAuth2 would be handled by an OAuth2Handler
                logger.debug("OAuth2 security scheme: {}", scheme.getName());
                break;

            case OPEN_ID_CONNECT:
                logger.debug("OpenID Connect security scheme: {}", scheme.getName());
                break;
        }
    }

    @Override
    @NotNull
    public IHttpClient getHttpClient() {
        return httpClient;
    }

    @Override
    @NotNull
    public IOpenAPIClient withSettings(@NotNull HttpSettings settings) {
        try {
            return new DesktopOpenAPIClient(specLocation, httpClient.withSettings(settings));
        } catch (IOException e) {
            throw new RuntimeException("Failed to create OpenAPI client with new settings", e);
        }
    }

    @Override
    @NotNull
    public String getOpenAPISpecLocation() {
        return specLocation;
    }
}
