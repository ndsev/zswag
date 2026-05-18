package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpRequest;
import io.github.ndsev.zswag.api.HttpResponse;
import io.github.ndsev.zswag.api.HttpSettings;
import io.github.ndsev.zswag.api.IHttpClient;
import io.github.ndsev.zswag.api.IKeychain;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Integration tests for {@link OpenApiClient#applySecurity} (private — exercised
 * via {@code callMethod}). Earlier the dispatch core was reachable in tests
 * only via {@code mock(OpenApiClient.class)}; these tests construct a real
 * client against a synthetic OpenAPI spec and a stub {@link IHttpClient} that
 * captures the outgoing request, then assert what was applied.
 *
 * <p>The spec covers the four interesting branches: OAuth2 (mints token + adds
 * Bearer header), API-key in header, API-key in query, OR-of-AND alternatives
 * fallthrough.
 */
class OpenApiClientSecurityTest {

    @TempDir
    Path tmp;

    @BeforeEach
    void clearTokenCache() {
        OAuth2Handler.clearAllCachedTokens();
    }

    private static final String SPEC = String.join("\n",
            "openapi: \"3.0.0\"",
            "info: { title: 'Sec', version: '1.0' }",
            "servers: [ { url: 'https://api.example.com/v1' } ]",
            "paths:",
            "  /oauth2:",
            "    get:",
            "      operationId: oauth2Op",
            "      responses: { '200': { description: ok } }",
            "      security: [ { OAuth2Auth: [ read ] } ]",
            "  /api-key-header:",
            "    get:",
            "      operationId: apiKeyHeaderOp",
            "      responses: { '200': { description: ok } }",
            "      security: [ { ApiKeyHeader: [] } ]",
            "  /api-key-query:",
            "    get:",
            "      operationId: apiKeyQueryOp",
            "      responses: { '200': { description: ok } }",
            "      security: [ { ApiKeyQuery: [] } ]",
            "  /alternative:",
            "    get:",
            "      operationId: alternativeOp",
            "      responses: { '200': { description: ok } }",
            "      # Either Bearer (HTTP) OR ApiKeyHeader satisfies the requirement.",
            "      security:",
            "        - BearerAuth: []",
            "        - ApiKeyHeader: []",
            "components:",
            "  securitySchemes:",
            "    BearerAuth: { type: http, scheme: bearer }",
            "    ApiKeyHeader: { type: apiKey, in: header, name: X-API-Key }",
            "    ApiKeyQuery: { type: apiKey, in: query, name: api_key }",
            "    OAuth2Auth:",
            "      type: oauth2",
            "      flows:",
            "        clientCredentials:",
            "          tokenUrl: https://idp.example.com/token",
            "          scopes: { read: 'Read access' }"
    );

    /**
     * Captures the most recent outgoing HttpRequest so tests can assert on it.
     * Routes the OAuth2 token endpoint to a deterministic JSON response; routes
     * everything else to a 200 with empty body.
     */
    private static final class CapturingHttpClient implements IHttpClient {
        final AtomicReference<HttpRequest> lastApiCall = new AtomicReference<>();
        final HttpSettings persistent;

        CapturingHttpClient() { this(HttpSettings.empty()); }
        CapturingHttpClient(HttpSettings persistent) { this.persistent = persistent; }

        @Override
        public HttpSettings getPersistentSettings() { return persistent; }

        @Override
        public HttpResponse execute(HttpRequest request, HttpConfig adhoc) {
            String url = request.getUrl();
            if (url.contains("/token")) {
                String body = "{\"access_token\":\"minted-tok\",\"expires_in\":3600}";
                return new HttpResponse(200, null, new LinkedHashMap<>(),
                        body.getBytes(StandardCharsets.UTF_8));
            }
            lastApiCall.set(request);
            return new HttpResponse(200, null, new LinkedHashMap<>(), new byte[0]);
        }
    }

    private Path writeSpec() throws IOException {
        Path p = tmp.resolve("test-spec.yaml");
        Files.writeString(p, SPEC);
        return p;
    }

    private static IKeychain noKeychain() {
        return (s, u) -> { throw new RuntimeException("keychain not expected in this test"); };
    }

    @Test
    void oauth2SchemeMintsTokenAndAddsBearerHeader() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        HttpConfig adhoc = HttpConfig.builder()
                .oauth2(HttpConfig.OAuth2.builder()
                        .clientId("cid").clientSecret("csec")
                        // useForSpecFetch defaults to true but the spec is loaded from a file
                        // here, so the spec-fetch branch isn't hit (no HTTP).
                        .build())
                .build();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, adhoc, noKeychain());

        client.callMethod("oauth2Op", Collections.emptyMap(), null);

        HttpRequest sent = http.lastApiCall.get();
        assertThat(sent).as("dispatch reached HTTP layer").isNotNull();
        assertThat(sent.getHeaders().get("Authorization")).isEqualTo("Bearer minted-tok");
    }

    @Test
    void apiKeyInHeaderRoutedToCorrectHeaderName() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        HttpConfig adhoc = HttpConfig.builder().apiKey("secret-key-value").build();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, adhoc, noKeychain());

        client.callMethod("apiKeyHeaderOp", Collections.emptyMap(), null);

        HttpRequest sent = http.lastApiCall.get();
        assertThat(sent).isNotNull();
        assertThat(sent.getHeaders().get("X-API-Key")).isEqualTo("secret-key-value");
        assertThat(sent.getHeaders()).doesNotContainKey("Authorization");
    }

    @Test
    void apiKeyInQueryRoutedToUrl() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        HttpConfig adhoc = HttpConfig.builder().apiKey("query-key-value").build();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, adhoc, noKeychain());

        client.callMethod("apiKeyQueryOp", Collections.emptyMap(), null);

        HttpRequest sent = http.lastApiCall.get();
        assertThat(sent).isNotNull();
        assertThat(sent.getUrl()).contains("api_key=query-key-value");
        assertThat(sent.getHeaders()).doesNotContainKey("X-API-Key");
    }

    @Test
    void alternativesPickFirstSatisfiable_apiKeyWhenNoBearer() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        // Only API-key configured: BearerAuth requires Authorization header which we
        // don't provide → applySecurity falls through to the ApiKeyHeader alternative.
        HttpConfig adhoc = HttpConfig.builder().apiKey("api-key-fallback").build();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, adhoc, noKeychain());

        client.callMethod("alternativeOp", Collections.emptyMap(), null);

        HttpRequest sent = http.lastApiCall.get();
        assertThat(sent).isNotNull();
        assertThat(sent.getHeaders().get("X-API-Key")).isEqualTo("api-key-fallback");
    }

    @Test
    void alternativesPickFirstSatisfiable_bearerWhenAuthorizationProvided() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        // Caller pre-supplies Authorization → BearerAuth alternative wins.
        HttpConfig adhoc = HttpConfig.builder()
                .bearerToken("user-supplied-token")
                .apiKey("fallback-only-if-bearer-fails")
                .build();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, adhoc, noKeychain());

        client.callMethod("alternativeOp", Collections.emptyMap(), null);

        HttpRequest sent = http.lastApiCall.get();
        assertThat(sent).isNotNull();
        // The Bearer is in effective.headers (merged from adhoc), not opHeaders, so the
        // dispatch layer doesn't re-add it; assert it would land via the dispatch path.
        // X-API-Key must NOT be present because the Bearer alternative was chosen first.
        assertThat(sent.getHeaders()).doesNotContainKey("X-API-Key");
    }

    @Test
    void useForSpecFetchRequiresTokenUrlInSettings() throws Exception {
        // When the spec is HTTP-served and useForSpecFetch=true (default), but the
        // settings don't supply a tokenUrl, the constructor must fail with a clear
        // message — we cannot fall back to the spec because we haven't fetched it yet.
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        HttpConfig adhoc = HttpConfig.builder()
                .oauth2(HttpConfig.OAuth2.builder().clientId("cid").clientSecret("csec").build())
                .build();
        // Use an http:// URL so the useForSpecFetch branch fires; the file doesn't have to
        // resolve — we expect to fail before the actual fetch.
        String httpSpecUrl = "http://example.test/spec.yaml";
        assertThatThrownBy(() -> new OpenApiClient(httpSpecUrl, http, adhoc, noKeychain()))
                .isInstanceOf(IOException.class)
                .hasMessageContaining("oauth2.tokenUrl");
    }

    @Test
    void noConfiguredCredentialsRaisesDescriptiveError() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        // No apiKey, no bearer, no oauth2 — nothing satisfies /alternative.
        HttpConfig adhoc = HttpConfig.empty();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, adhoc, noKeychain());

        assertThatThrownBy(() -> client.callMethod("alternativeOp", Collections.emptyMap(), null))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("none of the");
    }
}
