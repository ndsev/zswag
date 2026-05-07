package io.github.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Represents an HTTP request to be sent to a server.
 */
public class HttpRequest {
    private final String method;
    private final String url;
    private final Map<String, String> headers;
    private final byte[] body;

    private HttpRequest(String method, String url, Map<String, String> headers, byte[] body) {
        this.method = method;
        this.url = url;
        this.headers = headers != null ? Collections.unmodifiableMap(new HashMap<>(headers)) : Collections.emptyMap();
        this.body = body != null ? Arrays.copyOf(body, body.length) : null;
    }

    /**
     * @return HTTP method (GET, POST, PUT, DELETE, etc.)
     */
    @NotNull
    public String getMethod() {
        return method;
    }

    /**
     * @return Complete URL including scheme, host, path, and query string
     */
    @NotNull
    public String getUrl() {
        return url;
    }

    /**
     * @return HTTP headers as unmodifiable map
     */
    @NotNull
    public Map<String, String> getHeaders() {
        return headers;
    }

    /**
     * @return Request body as defensive copy (may be null for GET/DELETE)
     */
    @Nullable
    public byte[] getBody() {
        return body != null ? Arrays.copyOf(body, body.length) : null;
    }

    /**
     * Creates a new builder for constructing HttpRequest instances.
     */
    @NotNull
    public static Builder builder() {
        return new Builder();
    }

    /**
     * Builder for HttpRequest instances.
     */
    public static class Builder {
        private String method;
        private String url;
        private Map<String, String> headers = new HashMap<>();
        private byte[] body;

        private Builder() {
        }

        @NotNull
        public Builder method(@NotNull String method) {
            this.method = method;
            return this;
        }

        @NotNull
        public Builder url(@NotNull String url) {
            this.url = url;
            return this;
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
        public Builder body(@Nullable byte[] body) {
            this.body = body;
            return this;
        }

        @NotNull
        public HttpRequest build() {
            if (method == null || url == null) {
                throw new IllegalStateException("Method and URL are required");
            }
            return new HttpRequest(method, url, headers, body);
        }
    }
}
