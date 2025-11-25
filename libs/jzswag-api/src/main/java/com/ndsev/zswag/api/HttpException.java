package com.ndsev.zswag.api;

import org.jetbrains.annotations.Nullable;

/**
 * Exception thrown when HTTP communication fails.
 */
public class HttpException extends Exception {
    private final Integer statusCode;
    private final byte[] responseBody;

    public HttpException(@Nullable String message) {
        super(message);
        this.statusCode = null;
        this.responseBody = null;
    }

    public HttpException(@Nullable String message, @Nullable Throwable cause) {
        super(message, cause);
        this.statusCode = null;
        this.responseBody = null;
    }

    public HttpException(@Nullable String message, int statusCode, @Nullable byte[] responseBody) {
        super(message);
        this.statusCode = statusCode;
        this.responseBody = responseBody;
    }

    @Nullable
    public Integer getStatusCode() {
        return statusCode;
    }

    @Nullable
    public byte[] getResponseBody() {
        return responseBody;
    }
}
