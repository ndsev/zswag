package io.github.ndsev.zswag.jvm;

import io.github.ndsev.zswag.api.*;
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
     * Creates a ZswagServiceClient that uses the persistent {@link HttpSettings}
     * from the {@code HTTP_SETTINGS_FILE} environment variable.
     */
    @NotNull
    public static ZswagServiceClient create(@NotNull String serviceIdentifier, @NotNull String specLocation) throws IOException {
        return create(serviceIdentifier, specLocation, HttpSettingsLoader.loadFromEnvironment());
    }

    /**
     * Creates a ZswagServiceClient with explicit persistent settings.
     */
    @NotNull
    public static ZswagServiceClient create(@NotNull String serviceIdentifier, @NotNull String specLocation,
                                            @NotNull HttpSettings settings) throws IOException {
        IHttpClient httpClient = new JvmHttpClient(settings);
        IOpenAPIClient openAPIClient = new JvmOpenAPIClient(specLocation, httpClient);
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
     * Extracts parameters from the zserio service context. The context may contain
     * reflection objects with parameters. Throws {@link HttpException} on reflection
     * failure so the caller does not silently dispatch a request with a partial or
     * empty parameter map.
     *
     * <p>For the canonical typed entry point (Calculator.CalculatorClient(zswagClient)
     * via ZswagClient + ZserioReflection), this legacy path is unused. It exists for
     * direct {@link IZswagServiceClient#callMethod} consumers.
     */
    @NotNull
    private Map<String, Object> extractParameters(@NotNull Object context) throws HttpException {
        Map<String, Object> parameters = new HashMap<>();
        Class<?> contextClass = context.getClass();
        Method[] methods = contextClass.getMethods();

        for (Method method : methods) {
            String methodName = method.getName();
            // Skip Object.class accessors that aren't user-defined parameter getters.
            if (!methodName.startsWith("get") || method.getParameterCount() != 0) continue;
            if (method.getDeclaringClass() == Object.class) continue;
            String paramName = methodName.substring(3);
            if (paramName.isEmpty()) continue;
            paramName = Character.toLowerCase(paramName.charAt(0)) + paramName.substring(1);
            try {
                Object value = method.invoke(context);
                if (value != null) {
                    parameters.put(paramName, value);
                }
            } catch (ReflectiveOperationException e) {
                throw new HttpException("Failed to read parameter '" + paramName + "' from context "
                        + contextClass.getName() + ": " + e.getMessage(), e);
            }
        }
        return parameters;
    }

    @Override
    @NotNull
    public IOpenAPIClient getOpenAPIClient() {
        return openAPIClient;
    }

    @NotNull
    public String getServiceIdentifier() {
        return serviceIdentifier;
    }
}
