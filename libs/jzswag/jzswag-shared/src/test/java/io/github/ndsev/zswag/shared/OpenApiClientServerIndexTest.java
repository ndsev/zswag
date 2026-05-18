package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.HttpConfig;
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
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Multi-server support — matches C++ {@code OAClient(..., uint32_t serverIndex)}
 * and Python {@code OAClient(..., server_index=N)} (issue #113). zswag-Java's
 * {@link OpenApiClient} accepts a {@code serverIndex} constructor parameter
 * that selects which entry of the parsed {@code servers[]} array is used as
 * the base URL for dispatch.
 */
class OpenApiClientServerIndexTest {

    @TempDir
    Path tmp;

    private static final String SPEC = String.join("\n",
            "openapi: \"3.0.0\"",
            "info: { title: t, version: '1.0' }",
            "servers:",
            "  - url: 'https://primary.example.com/v1'",
            "  - url: 'https://secondary.example.com/v2'",
            "  - url: 'https://tertiary.example.com/v3'",
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

    private Path writeSpec() throws IOException {
        Path p = tmp.resolve("openapi.yaml");
        Files.writeString(p, SPEC);
        return p;
    }

    @Test
    void defaultIndexZeroPicksFirstServer() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        // The 4-arg constructor defaults serverIndex to 0.
        OpenApiClient client = new OpenApiClient(spec.toString(), http, HttpConfig.empty(), noKeychain());
        client.callMethod("ping", Collections.emptyMap(), null);
        assertThat(http.last.get().getUrl()).startsWith("https://primary.example.com/v1/ping");
    }

    @Test
    void explicitIndexOnePicksSecondServer() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, HttpConfig.empty(), noKeychain(), 1);
        client.callMethod("ping", Collections.emptyMap(), null);
        assertThat(http.last.get().getUrl()).startsWith("https://secondary.example.com/v2/ping");
    }

    @Test
    void explicitIndexTwoPicksThirdServer() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        OpenApiClient client = new OpenApiClient(spec.toString(), http, HttpConfig.empty(), noKeychain(), 2);
        client.callMethod("ping", Collections.emptyMap(), null);
        assertThat(http.last.get().getUrl()).startsWith("https://tertiary.example.com/v3/ping");
    }

    @Test
    void outOfBoundsIndexThrowsAtConstructionWithClearMessage() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        assertThatThrownBy(() ->
                new OpenApiClient(spec.toString(), http, HttpConfig.empty(), noKeychain(), 5))
                .isInstanceOf(IOException.class)
                .hasMessageContaining("serverIndex 5 is out of bounds")
                .hasMessageContaining("3 server(s)");
    }

    @Test
    void negativeIndexThrowsIllegalArgumentException() throws Exception {
        Path spec = writeSpec();
        CapturingHttpClient http = new CapturingHttpClient();
        assertThatThrownBy(() ->
                new OpenApiClient(spec.toString(), http, HttpConfig.empty(), noKeychain(), -1))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("serverIndex must be >= 0");
    }

    @Test
    void indexZeroValidEvenWhenServersArrayIsEmpty() throws Exception {
        // An empty servers array implies [{ "url": "/" }] per OpenAPI 3.0+ §4.7.5.
        // Index 0 should still be acceptable in that case.
        Path p = tmp.resolve("openapi.yaml");
        Files.writeString(p, String.join("\n",
                "openapi: \"3.0.0\"",
                "info: { title: t, version: '1.0' }",
                "servers: []",
                "paths: { /ping: { get: { operationId: ping, responses: { '200': { description: ok } } } } }"
        ));
        CapturingHttpClient http = new CapturingHttpClient();
        // Should NOT throw — index 0 is valid even with no declared servers.
        new OpenApiClient(p.toString(), http, HttpConfig.empty(), noKeychain(), 0);
    }
}
