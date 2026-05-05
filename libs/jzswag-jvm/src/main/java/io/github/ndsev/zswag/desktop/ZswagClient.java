package io.github.ndsev.zswag.desktop;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpSettings;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import zserio.runtime.ZserioError;
import zserio.runtime.io.Writer;
import zserio.runtime.service.ServiceClientInterface;
import zserio.runtime.service.ServiceData;

import java.io.IOException;

/**
 * The Java port of Python's {@code services.MyService.Client(OAClient(url))}
 * idiom. Implements zserio's {@link ServiceClientInterface} so that any
 * zserio-Java-generated {@code XClient} class accepts an instance of this
 * class as its transport.
 *
 * <p>Usage:
 * <pre>{@code
 * ZswagClient transport = new ZswagClient("http://api.example.com/openapi.json");
 * Calculator.CalculatorClient calc = new Calculator.CalculatorClient(transport);
 * Double result = calc.powerMethod(new BaseAndExponent(...));
 * }</pre>
 *
 * <p>Internally delegates to {@link DesktopOpenAPIClient}, which performs
 * {@code x-zserio-request-part} request decomposition via {@link ZserioReflection}.
 */
public final class ZswagClient implements ServiceClientInterface {
    private static final Logger logger = LoggerFactory.getLogger(ZswagClient.class);

    private final DesktopOpenAPIClient delegate;

    /**
     * Creates a client that uses persistent settings from {@code HTTP_SETTINGS_FILE}
     * and no adhoc config.
     */
    public ZswagClient(@NotNull String openApiSpecUrl) throws IOException {
        this(openApiSpecUrl, HttpSettingsLoader.loadFromEnvironment(), HttpConfig.empty());
    }

    /**
     * Creates a client with explicit persistent settings (typically loaded via
     * {@link HttpSettingsLoader}) and no adhoc config.
     */
    public ZswagClient(@NotNull String openApiSpecUrl, @NotNull HttpSettings persistent) throws IOException {
        this(openApiSpecUrl, persistent, HttpConfig.empty());
    }

    /**
     * Creates a client with explicit persistent settings AND a per-instance
     * adhoc {@link HttpConfig}. Mirrors the C++/Python pattern of passing
     * {@code httpcl::Config}/{@code HTTPConfig} into {@code OAClient}.
     */
    public ZswagClient(@NotNull String openApiSpecUrl, @NotNull HttpSettings persistent, @NotNull HttpConfig adhoc)
            throws IOException {
        DesktopHttpClient http = new DesktopHttpClient(persistent);
        this.delegate = new DesktopOpenAPIClient(openApiSpecUrl, http, adhoc);
    }

    /** Lower-level constructor — for tests / advanced use. */
    public ZswagClient(@NotNull DesktopOpenAPIClient delegate) {
        this.delegate = delegate;
    }

    /** Exposes the underlying OpenAPI client (read-only) for introspection. */
    @NotNull
    public DesktopOpenAPIClient getOpenAPIClient() {
        return delegate;
    }

    /**
     * Implementation of zserio's {@link ServiceClientInterface}: decomposes the
     * typed request, dispatches the HTTP call, returns response bytes.
     *
     * <p>The {@code requestData} carries both the serialized request bytes
     * ({@link ServiceData#getByteArray()}) and the typed object
     * ({@link ServiceData#getZserioObject()}); we use the typed object for
     * {@code x-zserio-request-part} resolution.
     */
    @Override
    public byte[] callMethod(java.lang.String methodName,
                             ServiceData<? extends Writer> requestData,
                             @Nullable java.lang.Object context) throws ZserioError {
        Writer typed = requestData.getZserioObject();
        if (typed == null) {
            throw new ZserioError("ZswagClient.callMethod: requestData.getZserioObject() returned null");
        }
        try {
            return delegate.callMethod(methodName, typed);
        } catch (HttpException e) {
            // Surface as ZserioError so that zserio-generated client code can propagate it
            // through its standard exception channel.
            ZserioError err = new ZserioError("ZswagClient: " + methodName + " failed: " + e.getMessage(), e);
            throw err;
        }
    }
}
