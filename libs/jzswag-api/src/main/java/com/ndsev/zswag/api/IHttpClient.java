package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;

/**
 * Interface for HTTP client implementations.
 * Platform-specific implementations handle actual HTTP communication.
 */
public interface IHttpClient {
    /**
     * Executes an HTTP request and returns the response.
     *
     * @param request The HTTP request to execute
     * @return The HTTP response
     * @throws HttpException if the request fails
     */
    @NotNull
    HttpResponse execute(@NotNull HttpRequest request) throws HttpException;

    /**
     * Gets the current HTTP settings for this client.
     *
     * @return The HTTP settings
     */
    @NotNull
    HttpSettings getSettings();

    /**
     * Creates a new HTTP client with updated settings.
     *
     * @param settings The new settings to use
     * @return A new HTTP client instance with the given settings
     */
    @NotNull
    IHttpClient withSettings(@NotNull HttpSettings settings);
}
