package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Represents an HTTP response received from a server.
 */
public class HttpResponse {
    private final int statusCode;
    private final String statusMessage;
    private final Map<String, String> headers;
    private final byte[] body;

    public HttpResponse(int statusCode, @Nullable String statusMessage,
                        @Nullable Map<String, String> headers, @Nullable byte[] body) {
        this.statusCode = statusCode;
        this.statusMessage = statusMessage;
        this.headers = headers != null ? Collections.unmodifiableMap(new HashMap<>(headers)) : Collections.emptyMap();
        this.body = body;
    }

    /**
     * @return HTTP status code (e.g., 200, 404, 500)
     */
    public int getStatusCode() {
        return statusCode;
    }

    /**
     * @return HTTP status message (e.g., "OK", "Not Found")
     */
    @Nullable
    public String getStatusMessage() {
        return statusMessage;
    }

    /**
     * @return Response headers as unmodifiable map
     */
    @NotNull
    public Map<String, String> getHeaders() {
        return headers;
    }

    /**
     * @return Response body (may be null)
     */
    @Nullable
    public byte[] getBody() {
        return body;
    }

    /**
     * @return true if status code is in the 2xx range
     */
    public boolean isSuccessful() {
        return statusCode >= 200 && statusCode < 300;
    }
}
