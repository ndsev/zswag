package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.HttpConfig;
import com.ndsev.zswag.api.HttpException;
import com.ndsev.zswag.api.HttpRequest;
import com.ndsev.zswag.api.HttpResponse;
import com.ndsev.zswag.api.IHttpClient;
import com.google.gson.Gson;
import com.google.gson.JsonObject;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Base64;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

/**
 * OAuth2 client credentials flow handler with token caching.
 * Thread-safe implementation with automatic token refresh.
 */
public class OAuth2Handler {
    private static final Logger logger = LoggerFactory.getLogger(OAuth2Handler.class);

    private final String tokenEndpoint;
    private final String clientId;
    private final String clientSecret;
    private final String scope;
    private final IHttpClient httpClient;
    private final Gson gson = new Gson();

    // Token cache
    private final ReadWriteLock lock = new ReentrantReadWriteLock();
    private String accessToken;
    private Instant tokenExpiry;

    public OAuth2Handler(@NotNull String tokenEndpoint, @NotNull String clientId,
                         @NotNull String clientSecret, @Nullable String scope,
                         @NotNull IHttpClient httpClient) {
        this.tokenEndpoint = tokenEndpoint;
        this.clientId = clientId;
        this.clientSecret = clientSecret;
        this.scope = scope;
        this.httpClient = httpClient;
    }

    /**
     * Gets a valid access token, refreshing if necessary.
     */
    @NotNull
    public String getAccessToken() throws HttpException {
        // Check if we have a valid cached token
        lock.readLock().lock();
        try {
            if (accessToken != null && tokenExpiry != null && Instant.now().isBefore(tokenExpiry)) {
                return accessToken;
            }
        } finally {
            lock.readLock().unlock();
        }

        // Token expired or not present, acquire new one
        lock.writeLock().lock();
        try {
            // Double-check after acquiring write lock
            if (accessToken != null && tokenExpiry != null && Instant.now().isBefore(tokenExpiry)) {
                return accessToken;
            }

            logger.info("Acquiring new OAuth2 access token from {}", tokenEndpoint);
            acquireToken();
            return accessToken;

        } finally {
            lock.writeLock().unlock();
        }
    }

    /**
     * Acquires a new access token using client credentials flow.
     */
    private void acquireToken() throws HttpException {
        // Build token request
        Map<String, String> formData = new HashMap<>();
        formData.put("grant_type", "client_credentials");
        if (scope != null) {
            formData.put("scope", scope);
        }

        String formBody = buildFormBody(formData);

        // Create Basic Auth header
        String credentials = clientId + ":" + clientSecret;
        String encodedCredentials = Base64.getEncoder().encodeToString(credentials.getBytes(StandardCharsets.UTF_8));

        HttpRequest request = HttpRequest.builder()
                .method("POST")
                .url(tokenEndpoint)
                .header("Authorization", "Basic " + encodedCredentials)
                .header("Content-Type", "application/x-www-form-urlencoded")
                .body(formBody.getBytes(StandardCharsets.UTF_8))
                .build();

        HttpResponse response = httpClient.execute(request, HttpConfig.empty());

        if (!response.isSuccessful()) {
            String error = response.getBody() != null ?
                    new String(response.getBody(), StandardCharsets.UTF_8) : "Unknown error";
            throw new HttpException("OAuth2 token request failed: " + error, response.getStatusCode(), response.getBody());
        }

        // Parse token response
        String responseBody = new String(response.getBody(), StandardCharsets.UTF_8);
        JsonObject tokenResponse = gson.fromJson(responseBody, JsonObject.class);

        accessToken = tokenResponse.get("access_token").getAsString();
        int expiresIn = tokenResponse.has("expires_in") ?
                tokenResponse.get("expires_in").getAsInt() : 3600;

        // Set expiry with 60 second buffer
        tokenExpiry = Instant.now().plusSeconds(expiresIn - 60);

        logger.info("Successfully acquired OAuth2 token (expires in {}s)", expiresIn);
    }

    /**
     * Builds a URL-encoded form body from parameters.
     */
    @NotNull
    private String buildFormBody(@NotNull Map<String, String> formData) {
        StringBuilder body = new StringBuilder();
        boolean first = true;
        for (Map.Entry<String, String> entry : formData.entrySet()) {
            if (!first) {
                body.append("&");
            }
            body.append(ParameterEncoder.urlEncode(entry.getKey()));
            body.append("=");
            body.append(ParameterEncoder.urlEncode(entry.getValue()));
            first = false;
        }
        return body.toString();
    }

    /**
     * Clears the cached token, forcing a refresh on next access.
     */
    public void clearToken() {
        lock.writeLock().lock();
        try {
            accessToken = null;
            tokenExpiry = null;
            logger.debug("OAuth2 token cache cleared");
        } finally {
            lock.writeLock().unlock();
        }
    }
}
