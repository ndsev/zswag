package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;

/**
 * Interface for zserio service clients that use OpenAPI for communication.
 * Provides a bridge between zserio services and OpenAPI endpoints.
 */
public interface IZswagServiceClient {
    /**
     * Calls a zserio service method.
     *
     * @param methodName The method name
     * @param requestData The serialized request data
     * @param context Optional context object (may contain parameters)
     * @return The serialized response data
     * @throws HttpException if the call fails
     */
    @NotNull
    byte[] callMethod(@NotNull String methodName, @NotNull byte[] requestData, @NotNull Object context)
            throws HttpException;

    /**
     * Gets the underlying OpenAPI client.
     *
     * @return The OpenAPI client
     */
    @NotNull
    IOpenAPIClient getOpenAPIClient();
}
