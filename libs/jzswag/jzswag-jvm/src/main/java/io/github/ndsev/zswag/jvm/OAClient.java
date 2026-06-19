package io.github.ndsev.zswag.jvm;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpSettings;
import io.github.ndsev.zswag.api.IKeychain;
import io.github.ndsev.zswag.shared.HttpSettingsLoader;
import io.github.ndsev.zswag.shared.OpenApiClient;
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
 * JVM Java port of Python's {@code services.MyService.Client(OAClient(url))}
 * idiom. Implements zserio's {@link ServiceClientInterface} so that any
 * zserio-Java-generated {@code XClient} class accepts an instance of this
 * class as its transport.
 *
 * <p>Usage:
 * <pre>{@code
 * OAClient transport = new OAClient("http://api.example.com/openapi.json");
 * Calculator.CalculatorClient calc = new Calculator.CalculatorClient(transport);
 * Double result = calc.powerMethod(new BaseAndExponent(...));
 * }</pre>
 *
 * <p>Internally delegates to {@link OpenApiClient}, which performs
 * {@code x-zserio-request-part} request decomposition.
 */
public final class OAClient implements ServiceClientInterface {
    private static final Logger logger = LoggerFactory.getLogger(OAClient.class);

    private final OpenApiClient delegate;

    /**
     * Creates a client that uses persistent settings from {@code HTTP_SETTINGS_FILE}.
     * Subsequent mtime changes to the settings file are picked up on the next request
     * (matches C++ {@code Settings::operator[]} hot-reload). Use the
     * {@link #OAClient(String, HttpSettings, HttpConfig)} form instead if you want
     * to pin a specific snapshot.
     */
    public OAClient(@NotNull String openApiSpecUrl) throws IOException {
        this(openApiSpecUrl, HttpConfig.empty(), 0);
    }

    /**
     * Env-driven constructor with an explicit {@code serverIndex}. Persistent
     * settings come from {@code HTTP_SETTINGS_FILE} via a {@link HttpSettingsLoader.HotReloader}
     * so file changes are picked up automatically.
     */
    public OAClient(@NotNull String openApiSpecUrl, @NotNull HttpConfig adhoc, int serverIndex)
            throws IOException {
        IKeychain keychain = new Keychain();
        // Package-private ctor: env-driven HotReloader so the source path is preserved
        // and mtime advances trigger an automatic reload on the next request.
        JvmHttpClient http = new JvmHttpClient(HttpSettingsLoader.HotReloader.fromEnvironment(), keychain);
        this.delegate = new OpenApiClient(openApiSpecUrl, http, adhoc, keychain, serverIndex);
    }

    /**
     * Creates a client with explicit persistent settings (typically loaded via
     * {@link HttpSettingsLoader}) and no adhoc config.
     */
    public OAClient(@NotNull String openApiSpecUrl, @NotNull HttpSettings persistent) throws IOException {
        this(openApiSpecUrl, persistent, HttpConfig.empty());
    }

    /**
     * Creates a client with explicit persistent settings AND a per-instance
     * adhoc {@link HttpConfig}. Mirrors the C++/Python pattern of passing
     * {@code httpcl::Config}/{@code HTTPConfig} into {@code OAClient}.
     */
    public OAClient(@NotNull String openApiSpecUrl, @NotNull HttpSettings persistent, @NotNull HttpConfig adhoc)
            throws IOException {
        this(openApiSpecUrl, persistent, adhoc, 0);
    }

    /**
     * Creates a client targeting a specific entry of the spec's
     * {@code servers[]} array. Mirrors C++ {@code OAClient(..., uint32_t serverIndex)}
     * and Python {@code OAClient(..., server_index=N)} — see issue #113.
     *
     * @param serverIndex index into the parsed {@code servers[]} array (default 0).
     *                    {@link IOException} is thrown during construction if the
     *                    index is out of bounds.
     */
    public OAClient(@NotNull String openApiSpecUrl, @NotNull HttpSettings persistent, @NotNull HttpConfig adhoc,
                    int serverIndex) throws IOException {
        IKeychain keychain = new Keychain();
        JvmHttpClient http = new JvmHttpClient(persistent, keychain);
        this.delegate = new OpenApiClient(openApiSpecUrl, http, adhoc, keychain, serverIndex);
    }

    /** Lower-level constructor — for tests / advanced use. */
    public OAClient(@NotNull OpenApiClient delegate) {
        this.delegate = delegate;
    }

    /** Exposes the underlying OpenAPI client (read-only) for introspection. */
    @NotNull
    public OpenApiClient getOpenApiClient() {
        return delegate;
    }

    /**
     * Implementation of zserio's {@link ServiceClientInterface}: decomposes the
     * typed request, dispatches the HTTP call, returns response bytes.
     */
    @Override
    public byte[] callMethod(java.lang.String methodName,
                             ServiceData<? extends Writer> requestData,
                             @Nullable java.lang.Object context) throws ZserioError {
        Writer typed = requestData.getZserioObject();
        if (typed == null) {
            throw new ZserioError("OAClient.callMethod: requestData.getZserioObject() returned null");
        }
        try {
            return delegate.callMethod(methodName, typed);
        } catch (HttpException e) {
            ZserioError err = new ZserioError("OAClient: " + methodName + " failed: " + e.getMessage(), e);
            throw err;
        }
    }
}
