package io.github.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.util.Map;

/**
 * Interface for OpenAPI-compliant clients.
 * Provides methods for calling OpenAPI endpoints with automatic parameter encoding
 * and authentication handling.
 */
public interface IOpenApiClient {
    /**
     * Calls an OpenAPI method with the given parameters.
     *
     * @param methodPath The OpenAPI method path (e.g., "/users/{id}")
     * @param parameters Map of parameter names to values
     * @param requestBody Optional request body (zserio binary or null)
     * @return The response body as byte array
     * @throws HttpException if the call fails
     */
    @Nullable
    byte[] callMethod(@NotNull String methodPath,
                      @NotNull Map<String, Object> parameters,
                      @Nullable byte[] requestBody) throws HttpException;

    /**
     * Gets the underlying HTTP client.
     *
     * @return The HTTP client
     */
    @NotNull
    IHttpClient getHttpClient();

    /**
     * Gets the OpenAPI specification URL or file path.
     *
     * @return The OpenAPI spec location
     */
    @NotNull
    String getOpenAPISpecLocation();
}
