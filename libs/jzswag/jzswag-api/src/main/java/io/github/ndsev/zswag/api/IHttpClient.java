package io.github.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;

/**
 * Platform-agnostic HTTP client interface. Implementations are responsible for
 * applying both their persistent {@link HttpSettings} (scope-matched against the
 * request URL) and the per-call {@code adhoc} {@link HttpConfig} to the request
 * before dispatch. Mirrors the C++ {@code httpcl::IHttpClient} contract.
 */
public interface IHttpClient {
    /**
     * Executes an HTTP request and returns the response. The {@code adhoc} config
     * is merged on top of the implementation's persistent settings (scope-matched
     * against {@link HttpRequest#getUrl()}).
     *
     * @param request The HTTP request to execute
     * @param adhoc Per-call configuration (use {@link HttpConfig#empty()} for none)
     * @return The HTTP response
     * @throws HttpException if the request fails
     */
    @NotNull
    HttpResponse execute(@NotNull HttpRequest request, @NotNull HttpConfig adhoc) throws HttpException;

    /**
     * Returns the persistent settings registry this client applies on every
     * request. Exposed so that higher layers (e.g. the OpenAPI dispatch core)
     * can compute the effective {@link HttpConfig} for a URL without having to
     * downcast to a platform-specific implementation.
     *
     * <p>Default returns {@link HttpSettings#empty()} so simple lambda-based
     * implementations (e.g. test stubs) don't need to override.
     */
    @NotNull
    default HttpSettings getPersistentSettings() {
        return HttpSettings.empty();
    }
}
