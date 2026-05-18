package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpRequest;
import io.github.ndsev.zswag.api.HttpResponse;
import io.github.ndsev.zswag.api.HttpSettings;
import io.github.ndsev.zswag.api.IHttpClient;
import io.github.ndsev.zswag.api.IKeychain;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.concurrent.atomic.AtomicReference;

import static org.assertj.core.api.Assertions.assertThat;

/**
 * Verifies that {@code OpenApiClient.resolveBaseUrl} resolves
 * {@code servers[0].url} against the spec location, per OpenAPI 3.0+
 * (clarified in OpenAPI 3.2.0 §4.5.2.1). Covers the three permitted reference
 * forms — absolute, server-relative, document-relative — and the spec-URL
 * scheme variants (http, https, local file path, file:// URL).
 */
class OpenApiClientBaseUrlTest {

    @TempDir
    Path tmp;

    // ------------------------------------------------------------------------
    // Unit-level: static resolveBaseUrl exercises every branch directly.
    // ------------------------------------------------------------------------

    @Test
    void absoluteServerUrlReturnedAsIs() {
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                "https://other.example.com/api"))
                .isEqualTo("https://other.example.com/api");
    }

    @Test
    void serverRelativePath_inheritsSpecSchemeAndHost() {
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                "/v2"))
                .isEqualTo("https://api.example.com/v2");
    }

    @Test
    void documentRelativeDot_resolvesToSpecDirectory() {
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                "."))
                .isEqualTo("https://api.example.com/v1/");
    }

    @Test
    void documentRelativeDotSlashV2_appendsToSpecDirectory() {
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                "./v2"))
                .isEqualTo("https://api.example.com/v1/v2");
    }

    @Test
    void documentRelativeDotDotSlashV2_goesUpOneDirectory() {
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                "../v2"))
                .isEqualTo("https://api.example.com/v2");
    }

    @Test
    void documentRelativeBareV2_treatedAsDotSlashV2() {
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                "v2"))
                .isEqualTo("https://api.example.com/v1/v2");
    }

    @Test
    void emptyServersDefaultsToRoot() {
        // The instance method substitutes "/" for an empty servers array;
        // here we just confirm the static method handles "/" correctly.
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                "/"))
                .isEqualTo("https://api.example.com/");
    }

    @Test
    void portCarriesIntoResolvedUrl() {
        assertThat(OpenApiClient.resolveBaseUrl(
                "http://localhost:8080/api/openapi.json",
                "."))
                .isEqualTo("http://localhost:8080/api/");
    }

    @Test
    void fileSpecWithAbsoluteServerUrl_returnsAbsoluteAsIs() {
        // Local-file spec is fine for dispatch as long as the server URL is absolute.
        assertThat(OpenApiClient.resolveBaseUrl(
                "/tmp/specs/openapi.yaml",
                "https://api.example.com/v1"))
                .isEqualTo("https://api.example.com/v1");
    }

    @Test
    void fileSpecWithRelativeServerUrl_returnsFileUriWithWarning() {
        // Edge case: a local-file spec + relative server URL produces a file:// base.
        // The method logs a warning and returns the resolved file URI verbatim.
        // This is almost never useful but at least is deterministic.
        String resolved = OpenApiClient.resolveBaseUrl(
                "/tmp/specs/openapi.yaml",
                ".");
        assertThat(resolved).startsWith("file:/").endsWith("/tmp/specs/");
    }

    @Test
    void fileUriSpecWithDocumentRelativeServer() {
        // java.net.URI.toString collapses an empty authority to "file:/...".
        // RFC 3986 considers "file:/x" and "file:///x" equivalent.
        assertThat(OpenApiClient.resolveBaseUrl(
                "file:///tmp/specs/openapi.yaml",
                "./v2"))
                .isEqualTo("file:/tmp/specs/v2");
    }

    @Test
    void invalidServerUrl_returnedVerbatimWithWarning() {
        // A malformed URI reference shouldn't crash — log and return as-is.
        String malformed = "bad scheme://[ here";
        assertThat(OpenApiClient.resolveBaseUrl(
                "https://api.example.com/v1/openapi.json",
                malformed))
                .isEqualTo(malformed);
    }

    // ------------------------------------------------------------------------
    // End-to-end: a real OpenApiClient is built from a temp-file spec and the
    // dispatch URL is observed. Confirms resolveBaseUrl is wired through to
    // dispatch (not just a standalone function).
    // ------------------------------------------------------------------------

    private static final String SPEC_TEMPLATE = String.join("\n",
            "openapi: \"3.0.0\"",
            "info: { title: t, version: '1.0' }",
            "%SERVERS%",
            "paths:",
            "  /ping:",
            "    get:",
            "      operationId: ping",
            "      responses: { '200': { description: ok } }"
    );

    private static final class CapturingHttpClient implements IHttpClient {
        final AtomicReference<HttpRequest> last = new AtomicReference<>();

        @Override
        public HttpSettings getPersistentSettings() { return HttpSettings.empty(); }

        @Override
        public HttpResponse execute(HttpRequest request, HttpConfig adhoc) {
            last.set(request);
            return new HttpResponse(200, null, new LinkedHashMap<>(), new byte[0]);
        }
    }

    private static IKeychain noKeychain() {
        return (s, u) -> { throw new RuntimeException("keychain not expected"); };
    }

    private Path writeSpec(String serversBlock) throws IOException {
        Path p = tmp.resolve("openapi.yaml");
        Files.writeString(p, SPEC_TEMPLATE.replace("%SERVERS%", serversBlock));
        return p;
    }

    @Test
    void e2e_absoluteServerWithLocalFileSpec_dispatchHitsAbsoluteUrl() throws Exception {
        Path spec = writeSpec("servers: [ { url: 'https://api.example.com/v1' } ]");
        CapturingHttpClient http = new CapturingHttpClient();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, HttpConfig.empty(), noKeychain());
        client.callMethod("ping", Collections.emptyMap(), null);
        HttpRequest sent = http.last.get();
        assertThat(sent).isNotNull();
        assertThat(sent.getUrl()).startsWith("https://api.example.com/v1/ping");
    }
}
