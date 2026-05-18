package io.github.ndsev.zswag.android;

import android.content.Context;
import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpSettings;
import io.github.ndsev.zswag.api.IKeychain;
import io.github.ndsev.zswag.shared.HttpSettingsLoader;
import io.github.ndsev.zswag.shared.OpenAPIClient;
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
 * Android counterpart of the JVM {@code OAClient}: implements zserio's
 * {@link ServiceClientInterface} so any zserio-Java-generated {@code XClient}
 * accepts an instance as its transport.
 *
 * <p>The only public-API difference from the JVM port is the {@link Context}
 * parameter on the convenience constructors — needed so {@link AndroidKeychain}
 * can reach {@link android.content.SharedPreferences} for credential storage.
 *
 * <p>Usage:
 * <pre>{@code
 * OAClient transport = new OAClient(context, "https://api.example.com/openapi.json");
 * Calculator.CalculatorClient calc = new Calculator.CalculatorClient(transport);
 * Double result = calc.powerMethod(new BaseAndExponent(...));
 * }</pre>
 */
public final class OAClient implements ServiceClientInterface {
    private static final Logger logger = LoggerFactory.getLogger(OAClient.class);

    private final OpenAPIClient delegate;

    /**
     * Creates a client that uses persistent settings from {@code HTTP_SETTINGS_FILE}
     * and no adhoc config.
     */
    public OAClient(@NotNull Context context, @NotNull String openApiSpecUrl) throws IOException {
        this(context, openApiSpecUrl, HttpSettingsLoader.loadFromEnvironment(), HttpConfig.empty());
    }

    /**
     * Creates a client with explicit persistent settings (typically loaded via
     * {@link HttpSettingsLoader}) and no adhoc config.
     */
    public OAClient(@NotNull Context context, @NotNull String openApiSpecUrl,
                       @NotNull HttpSettings persistent) throws IOException {
        this(context, openApiSpecUrl, persistent, HttpConfig.empty());
    }

    /**
     * Creates a client with explicit persistent settings AND a per-instance
     * adhoc {@link HttpConfig}.
     */
    public OAClient(@NotNull Context context, @NotNull String openApiSpecUrl,
                       @NotNull HttpSettings persistent, @NotNull HttpConfig adhoc) throws IOException {
        AndroidLogging.init();
        IKeychain keychain = new AndroidKeychain(context);
        AndroidHttpClient http = new AndroidHttpClient(persistent, keychain);
        this.delegate = new OpenAPIClient(openApiSpecUrl, http, adhoc, keychain);
    }

    /** Lower-level constructor — for tests / advanced use. */
    public OAClient(@NotNull OpenAPIClient delegate) {
        this.delegate = delegate;
    }

    /** Exposes the underlying OpenAPI client (read-only) for introspection. */
    @NotNull
    public OpenAPIClient getOpenAPIClient() {
        return delegate;
    }

    @Override
    public byte[] callMethod(java.lang.String methodName,
                             ServiceData<? extends Writer> requestData,
                             @Nullable java.lang.Object zserioContext) throws ZserioError {
        Writer typed = requestData.getZserioObject();
        if (typed == null) {
            throw new ZserioError("OAClient.callMethod: requestData.getZserioObject() returned null");
        }
        try {
            return delegate.callMethod(methodName, typed);
        } catch (HttpException e) {
            throw new ZserioError("OAClient: " + methodName + " failed: " + e.getMessage(), e);
        }
    }
}
