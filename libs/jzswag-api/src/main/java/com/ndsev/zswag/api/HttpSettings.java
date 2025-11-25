package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.time.Duration;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * HTTP client configuration settings.
 * This class is immutable and uses the builder pattern for construction.
 */
public class HttpSettings {
    private final Map<String, String> headers;
    private final Map<String, String> queryParameters;
    private final Map<String, String> cookies;
    private final Duration timeout;
    private final boolean sslStrict;
    private final String proxyUrl;
    private final String basicAuthUsername;
    private final String basicAuthPassword;
    private final String bearerToken;
    private final Map<String, String> apiKeys;

    private HttpSettings(Builder builder) {
        this.headers = Collections.unmodifiableMap(new HashMap<>(builder.headers));
        this.queryParameters = Collections.unmodifiableMap(new HashMap<>(builder.queryParameters));
        this.cookies = Collections.unmodifiableMap(new HashMap<>(builder.cookies));
        this.timeout = builder.timeout;
        this.sslStrict = builder.sslStrict;
        this.proxyUrl = builder.proxyUrl;
        this.basicAuthUsername = builder.basicAuthUsername;
        this.basicAuthPassword = builder.basicAuthPassword;
        this.bearerToken = builder.bearerToken;
        this.apiKeys = Collections.unmodifiableMap(new HashMap<>(builder.apiKeys));
    }

    @NotNull
    public Map<String, String> getHeaders() {
        return headers;
    }

    @NotNull
    public Map<String, String> getQueryParameters() {
        return queryParameters;
    }

    @NotNull
    public Map<String, String> getCookies() {
        return cookies;
    }

    @NotNull
    public Duration getTimeout() {
        return timeout;
    }

    public boolean isSslStrict() {
        return sslStrict;
    }

    @Nullable
    public String getProxyUrl() {
        return proxyUrl;
    }

    @Nullable
    public String getBasicAuthUsername() {
        return basicAuthUsername;
    }

    @Nullable
    public String getBasicAuthPassword() {
        return basicAuthPassword;
    }

    @Nullable
    public String getBearerToken() {
        return bearerToken;
    }

    @NotNull
    public Map<String, String> getApiKeys() {
        return apiKeys;
    }

    @NotNull
    public static Builder builder() {
        return new Builder();
    }

    /**
     * Creates a new builder initialized with this settings' values.
     */
    @NotNull
    public Builder toBuilder() {
        return new Builder(this);
    }

    public static class Builder {
        private Map<String, String> headers = new HashMap<>();
        private Map<String, String> queryParameters = new HashMap<>();
        private Map<String, String> cookies = new HashMap<>();
        private Duration timeout = Duration.ofSeconds(30);
        private boolean sslStrict = true;
        private String proxyUrl;
        private String basicAuthUsername;
        private String basicAuthPassword;
        private String bearerToken;
        private Map<String, String> apiKeys = new HashMap<>();

        private Builder() {
        }

        private Builder(HttpSettings settings) {
            this.headers = new HashMap<>(settings.headers);
            this.queryParameters = new HashMap<>(settings.queryParameters);
            this.cookies = new HashMap<>(settings.cookies);
            this.timeout = settings.timeout;
            this.sslStrict = settings.sslStrict;
            this.proxyUrl = settings.proxyUrl;
            this.basicAuthUsername = settings.basicAuthUsername;
            this.basicAuthPassword = settings.basicAuthPassword;
            this.bearerToken = settings.bearerToken;
            this.apiKeys = new HashMap<>(settings.apiKeys);
        }

        @NotNull
        public Builder header(@NotNull String name, @NotNull String value) {
            this.headers.put(name, value);
            return this;
        }

        @NotNull
        public Builder headers(@NotNull Map<String, String> headers) {
            this.headers.putAll(headers);
            return this;
        }

        @NotNull
        public Builder queryParameter(@NotNull String name, @NotNull String value) {
            this.queryParameters.put(name, value);
            return this;
        }

        @NotNull
        public Builder queryParameters(@NotNull Map<String, String> queryParameters) {
            this.queryParameters.putAll(queryParameters);
            return this;
        }

        @NotNull
        public Builder cookie(@NotNull String name, @NotNull String value) {
            this.cookies.put(name, value);
            return this;
        }

        @NotNull
        public Builder cookies(@NotNull Map<String, String> cookies) {
            this.cookies.putAll(cookies);
            return this;
        }

        @NotNull
        public Builder timeout(@NotNull Duration timeout) {
            this.timeout = timeout;
            return this;
        }

        @NotNull
        public Builder sslStrict(boolean sslStrict) {
            this.sslStrict = sslStrict;
            return this;
        }

        @NotNull
        public Builder proxyUrl(@Nullable String proxyUrl) {
            this.proxyUrl = proxyUrl;
            return this;
        }

        @NotNull
        public Builder basicAuth(@NotNull String username, @NotNull String password) {
            this.basicAuthUsername = username;
            this.basicAuthPassword = password;
            return this;
        }

        @NotNull
        public Builder bearerToken(@NotNull String token) {
            this.bearerToken = token;
            return this;
        }

        @NotNull
        public Builder apiKey(@NotNull String name, @NotNull String value) {
            this.apiKeys.put(name, value);
            return this;
        }

        @NotNull
        public Builder apiKeys(@NotNull Map<String, String> apiKeys) {
            this.apiKeys.putAll(apiKeys);
            return this;
        }

        @NotNull
        public HttpSettings build() {
            return new HttpSettings(this);
        }
    }
}
