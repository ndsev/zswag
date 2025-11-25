package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Base64;
import java.util.Map;
import java.util.StringJoiner;

/**
 * Desktop implementation of IHttpClient using Java 11 HttpClient.
 */
public class DesktopHttpClient implements IHttpClient {
    private static final Logger logger = LoggerFactory.getLogger(DesktopHttpClient.class);

    private final HttpClient httpClient;
    private final HttpSettings settings;

    public DesktopHttpClient(@NotNull HttpSettings settings) {
        this.settings = settings;
        this.httpClient = createHttpClient(settings);
    }

    /**
     * Creates a Java 11 HttpClient configured with the given settings.
     */
    @NotNull
    private static HttpClient createHttpClient(@NotNull HttpSettings settings) {
        HttpClient.Builder builder = HttpClient.newBuilder()
                .version(HttpClient.Version.HTTP_1_1)
                .followRedirects(HttpClient.Redirect.NORMAL)
                .connectTimeout(settings.getTimeout());

        // TODO: Add proxy support
        // TODO: Add SSL configuration

        return builder.build();
    }

    @Override
    @NotNull
    public com.ndsev.zswag.api.HttpResponse execute(@NotNull com.ndsev.zswag.api.HttpRequest request)
            throws HttpException {
        try {
            logger.debug("Executing {} request to {}", request.getMethod(), request.getUrl());

            // Build the Java HttpRequest
            HttpRequest.Builder requestBuilder = HttpRequest.newBuilder()
                    .uri(URI.create(request.getUrl()))
                    .timeout(settings.getTimeout());

            // Add headers from request
            for (Map.Entry<String, String> header : request.getHeaders().entrySet()) {
                requestBuilder.header(header.getKey(), header.getValue());
            }

            // Add headers from settings
            for (Map.Entry<String, String> header : settings.getHeaders().entrySet()) {
                requestBuilder.header(header.getKey(), header.getValue());
            }

            // Add cookies from settings as Cookie header
            Map<String, String> cookies = settings.getCookies();
            if (!cookies.isEmpty()) {
                StringJoiner cookieJoiner = new StringJoiner("; ");
                for (Map.Entry<String, String> cookie : cookies.entrySet()) {
                    cookieJoiner.add(cookie.getKey() + "=" + cookie.getValue());
                }
                requestBuilder.header("Cookie", cookieJoiner.toString());
            }

            // Add authentication headers
            addAuthenticationHeaders(requestBuilder);

            // Set HTTP method and body
            switch (request.getMethod().toUpperCase()) {
                case "GET":
                    requestBuilder.GET();
                    break;
                case "POST":
                    if (request.getBody() != null) {
                        requestBuilder.POST(HttpRequest.BodyPublishers.ofByteArray(request.getBody()));
                    } else {
                        requestBuilder.POST(HttpRequest.BodyPublishers.noBody());
                    }
                    break;
                case "PUT":
                    if (request.getBody() != null) {
                        requestBuilder.PUT(HttpRequest.BodyPublishers.ofByteArray(request.getBody()));
                    } else {
                        requestBuilder.PUT(HttpRequest.BodyPublishers.noBody());
                    }
                    break;
                case "DELETE":
                    requestBuilder.DELETE();
                    break;
                case "PATCH":
                    if (request.getBody() != null) {
                        requestBuilder.method("PATCH", HttpRequest.BodyPublishers.ofByteArray(request.getBody()));
                    } else {
                        requestBuilder.method("PATCH", HttpRequest.BodyPublishers.noBody());
                    }
                    break;
                default:
                    throw new HttpException("Unsupported HTTP method: " + request.getMethod());
            }

            HttpRequest httpRequest = requestBuilder.build();

            // Execute the request
            HttpResponse<byte[]> response = httpClient.send(httpRequest, HttpResponse.BodyHandlers.ofByteArray());

            logger.debug("Received response with status code: {}", response.statusCode());

            // Convert to our HttpResponse
            return new com.ndsev.zswag.api.HttpResponse(
                    response.statusCode(),
                    null, // Java HttpClient doesn't expose status message
                    convertHeaders(response.headers().map()),
                    response.body()
            );

        } catch (IOException e) {
            logger.error("HTTP request failed: {}", e.getMessage(), e);
            throw new HttpException("HTTP request failed: " + e.getMessage(), e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            logger.error("HTTP request interrupted: {}", e.getMessage(), e);
            throw new HttpException("HTTP request interrupted: " + e.getMessage(), e);
        }
    }

    /**
     * Adds authentication headers based on settings.
     * Note: Bearer token takes precedence over Basic auth if both are configured.
     */
    private void addAuthenticationHeaders(@NotNull HttpRequest.Builder requestBuilder) {
        // Bearer token takes precedence over Basic auth
        if (settings.getBearerToken() != null) {
            requestBuilder.header("Authorization", "Bearer " + settings.getBearerToken());
        } else if (settings.getBasicAuthUsername() != null && settings.getBasicAuthPassword() != null) {
            // Basic authentication (only if no bearer token)
            String credentials = settings.getBasicAuthUsername() + ":" + settings.getBasicAuthPassword();
            String encodedCredentials = Base64.getEncoder().encodeToString(
                    credentials.getBytes(StandardCharsets.UTF_8));
            requestBuilder.header("Authorization", "Basic " + encodedCredentials);
        }

        // API keys are added to headers by the OpenAPIClient based on security scheme definition
    }

    /**
     * Converts Java HttpHeaders map to a simple String map.
     */
    @NotNull
    private Map<String, String> convertHeaders(@NotNull Map<String, java.util.List<String>> headersMap) {
        Map<String, String> result = new java.util.HashMap<>();
        for (Map.Entry<String, java.util.List<String>> entry : headersMap.entrySet()) {
            if (!entry.getValue().isEmpty()) {
                // Take the first value if multiple exist
                result.put(entry.getKey(), entry.getValue().get(0));
            }
        }
        return result;
    }

    @Override
    @NotNull
    public HttpSettings getSettings() {
        return settings;
    }

    @Override
    @NotNull
    public IHttpClient withSettings(@NotNull HttpSettings settings) {
        return new DesktopHttpClient(settings);
    }
}
