package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import zserio.runtime.io.ByteArrayBitStreamReader;
import zserio.runtime.io.ByteArrayBitStreamWriter;
import zserio.runtime.io.SerializeUtil;

import java.io.IOException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

/**
 * zserio service client implementation that uses OpenAPI for communication.
 * Implements the zserio ServiceInterface to integrate with zserio services.
 */
public class ZswagServiceClient implements IZswagServiceClient {
    private static final Logger logger = LoggerFactory.getLogger(ZswagServiceClient.class);

    private final IOpenAPIClient openAPIClient;
    private final String serviceIdentifier;

    public ZswagServiceClient(@NotNull String serviceIdentifier, @NotNull IOpenAPIClient openAPIClient) {
        this.serviceIdentifier = serviceIdentifier;
        this.openAPIClient = openAPIClient;
    }

    /**
     * Creates a ZswagServiceClient from an OpenAPI spec location.
     */
    @NotNull
    public static ZswagServiceClient create(@NotNull String serviceIdentifier, @NotNull String specLocation,
                                            @NotNull HttpSettings settings) throws IOException {
        IHttpClient httpClient = new DesktopHttpClient(settings);
        IOpenAPIClient openAPIClient = new DesktopOpenAPIClient(specLocation, httpClient);
        return new ZswagServiceClient(serviceIdentifier, openAPIClient);
    }

    @Override
    @NotNull
    public byte[] callMethod(@NotNull String methodName, @NotNull byte[] requestData, @NotNull Object context)
            throws HttpException {
        try {
            logger.debug("Calling zserio method: {}.{}", serviceIdentifier, methodName);

            // Build the method path for OpenAPI
            String methodPath = "/" + serviceIdentifier.replace(".", "/") + "/" + methodName;

            // Extract parameters from context if it's a reflection object
            Map<String, Object> parameters = extractParameters(context);

            // Call the OpenAPI method
            byte[] responseData = openAPIClient.callMethod(methodPath, parameters, requestData);

            if (responseData == null) {
                responseData = new byte[0];
            }

            return responseData;

        } catch (HttpException e) {
            logger.error("HTTP call failed for {}.{}: {}", serviceIdentifier, methodName, e.getMessage());
            throw e;
        }
    }

    /**
     * Extracts parameters from the zserio service context.
     * The context may contain reflection objects with parameters.
     */
    @NotNull
    private Map<String, Object> extractParameters(@NotNull Object context) {
        Map<String, Object> parameters = new HashMap<>();

        // Use reflection to extract parameters from the context object
        // This would need to be customized based on the zserio-generated types
        try {
            Class<?> contextClass = context.getClass();
            Method[] methods = contextClass.getMethods();

            for (Method method : methods) {
                String methodName = method.getName();
                // Look for getter methods
                if (methodName.startsWith("get") && method.getParameterCount() == 0) {
                    String paramName = methodName.substring(3);
                    if (!paramName.isEmpty()) {
                        paramName = Character.toLowerCase(paramName.charAt(0)) + paramName.substring(1);
                        Object value = method.invoke(context);
                        if (value != null) {
                            parameters.put(paramName, value);
                        }
                    }
                }
            }
        } catch (Exception e) {
            logger.debug("Could not extract parameters from context: {}", e.getMessage());
        }

        return parameters;
    }

    @Override
    @NotNull
    public IOpenAPIClient getOpenAPIClient() {
        return openAPIClient;
    }

    @Override
    @NotNull
    public IZswagServiceClient withSettings(@NotNull HttpSettings settings) {
        return new ZswagServiceClient(serviceIdentifier, openAPIClient.withSettings(settings));
    }

    @NotNull
    public String getServiceIdentifier() {
        return serviceIdentifier;
    }
}
