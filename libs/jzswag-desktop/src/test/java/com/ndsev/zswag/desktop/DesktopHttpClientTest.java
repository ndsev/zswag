package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.HttpConfig;
import com.ndsev.zswag.api.HttpException;
import com.ndsev.zswag.api.HttpRequest;
import com.ndsev.zswag.api.HttpResponse;
import com.ndsev.zswag.api.HttpSettings;
import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import okhttp3.mockwebserver.RecordedRequest;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.time.Duration;
import java.util.Collections;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class DesktopHttpClientTest {

    private MockWebServer server;

    @BeforeEach
    void start() throws IOException {
        server = new MockWebServer();
        server.start();
    }

    @AfterEach
    void stop() throws IOException {
        server.shutdown();
    }

    private DesktopHttpClient newClient() {
        return new DesktopHttpClient(HttpSettings.empty(), Duration.ofSeconds(5));
    }

    @Test
    void getRequestSendsRequestAndReturnsResponse() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200).setBody("hello"));
        HttpRequest req = HttpRequest.builder()
                .method("GET")
                .url(server.url("/path").toString())
                .build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(200);
        assertThat(new String(resp.getBody())).isEqualTo("hello");
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getMethod()).isEqualTo("GET");
        assertThat(recorded.getPath()).isEqualTo("/path");
    }

    @Test
    void postWithBodySendsBytes() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(201));
        byte[] body = "PAYLOAD".getBytes();
        HttpRequest req = HttpRequest.builder().method("POST").url(server.url("/p").toString()).body(body).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(201);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getMethod()).isEqualTo("POST");
        assertThat(recorded.getBody().readUtf8()).isEqualTo("PAYLOAD");
    }

    @Test
    void postWithoutBodySendsEmpty() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(204));
        HttpRequest req = HttpRequest.builder().method("POST").url(server.url("/p").toString()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(204);
    }

    @Test
    void putRequestSupported() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("PUT").url(server.url("/p").toString())
                .body("body".getBytes()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(200);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getMethod()).isEqualTo("PUT");
    }

    @Test
    void putWithoutBodyHasEmptyBody() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("PUT").url(server.url("/p").toString()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(200);
    }

    @Test
    void deleteRequestSupportedWithoutBody() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(204));
        HttpRequest req = HttpRequest.builder().method("DELETE").url(server.url("/p").toString()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(204);
        assertThat(server.takeRequest().getMethod()).isEqualTo("DELETE");
    }

    @Test
    void deleteRequestSupportedWithBody() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(204));
        HttpRequest req = HttpRequest.builder().method("DELETE").url(server.url("/p").toString())
                .body("payload".getBytes()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(204);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getMethod()).isEqualTo("DELETE");
        assertThat(recorded.getBody().readUtf8()).isEqualTo("payload");
    }

    @Test
    void unsupportedHttpMethodThrows() {
        HttpRequest req = HttpRequest.builder().method("PATCH").url(server.url("/x").toString()).build();
        assertThatThrownBy(() -> newClient().execute(req, HttpConfig.empty()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("Unsupported HTTP method");
    }

    @Test
    void perRequestHeadersTakePrecedenceOverAdhocHeaders() throws Exception {
        // Per-request Authorization should suppress an adhoc-config Authorization (avoiding
        // duplicate single-valued headers from JDK HttpRequest.Builder.header() append-semantics).
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString())
                .header("Authorization", "Bearer per-request").build();
        HttpConfig adhoc = HttpConfig.builder().bearerToken("from-adhoc").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getHeaders().values("Authorization")).containsExactly("Bearer per-request");
    }

    @Test
    void cookiesFromConfigAreSent() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder().cookie("a", "1").cookie("b", "2").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getHeader("Cookie")).contains("a=1").contains("b=2");
    }

    @Test
    void perRequestCookieHeaderSuppressesConfigCookies() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString())
                .header("Cookie", "explicit=yes").build();
        HttpConfig adhoc = HttpConfig.builder().cookie("a", "1").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getHeader("Cookie")).isEqualTo("explicit=yes");
    }

    @Test
    void basicAuthFromConfigInjectsAuthorizationHeader() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder().basicAuth("alice", "secret").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        // base64("alice:secret") = "YWxpY2U6c2VjcmV0"
        assertThat(recorded.getHeader("Authorization")).isEqualTo("Basic YWxpY2U6c2VjcmV0");
    }

    @Test
    void basicAuthSuppressedWhenAuthorizationAlreadyOnRequest() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString())
                .header("Authorization", "Bearer prebaked").build();
        HttpConfig adhoc = HttpConfig.builder().basicAuth("alice", "secret").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        // Per-request header wins; Basic from config not added
        assertThat(recorded.getHeaders().values("Authorization")).containsExactly("Bearer prebaked");
    }

    @Test
    void basicAuthSuppressedWhenAuthorizationInConfigHeaders() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder()
                .header("authorization", "Bearer x")  // case-insensitive check
                .basicAuth("alice", "secret")
                .build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getHeader("Authorization")).contains("Bearer x");
    }

    @Test
    void adhocHeadersFromConfigAreSent() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder()
                .addHeader("X-Multi", "v1")
                .addHeader("X-Multi", "v2")
                .build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getHeaders().values("X-Multi")).containsExactly("v1", "v2");
    }

    @Test
    void queryParametersAreAppendedToUrl() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder()
                .addQuery("a", "1")
                .addQuery("a", "2")
                .addQuery("b", "x y")
                .build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getPath()).contains("a=1").contains("a=2").contains("b=x+y");
    }

    @Test
    void queryParamsAppendedWithExistingQueryString() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p?fixed=yes").toString()).build();
        HttpConfig adhoc = HttpConfig.builder().query("extra", "1").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getPath()).contains("fixed=yes").contains("extra=1");
    }

    @Test
    void persistentSettingsAreScopeMergedAndAvailableForGetter() {
        HttpConfig wildcard = HttpConfig.builder()
                .scope("*", HttpSettings.compileScope("*"))
                .header("X-Default", "global")
                .build();
        HttpSettings persistent = new HttpSettings(Collections.singletonList(wildcard));
        DesktopHttpClient client = new DesktopHttpClient(persistent, Duration.ofSeconds(5));
        assertThat(client.getPersistentSettings()).isSameAs(persistent);
    }

    @Test
    void persistentScopeMatchesAndAddsHeaders() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        String url = server.url("/p").toString();
        HttpConfig wildcard = HttpConfig.builder()
                .scope("*", HttpSettings.compileScope("*"))
                .header("X-Default", "yes")
                .build();
        DesktopHttpClient client = new DesktopHttpClient(
                new HttpSettings(Collections.singletonList(wildcard)), Duration.ofSeconds(5));
        HttpRequest req = HttpRequest.builder().method("GET").url(url).build();
        client.execute(req, HttpConfig.empty());
        assertThat(server.takeRequest().getHeader("X-Default")).isEqualTo("yes");
    }

    @Test
    void connectionFailureSurfacesAsHttpException() {
        // Pick an unused port (server.shutdown later not needed)
        HttpRequest req = HttpRequest.builder().method("GET").url("http://127.0.0.1:1/x").build();
        assertThatThrownBy(() -> newClient().execute(req, HttpConfig.empty()))
                .isInstanceOf(HttpException.class);
    }

    @Test
    void defaultConstructorReadsEnvButYieldsValidClient() {
        // Stripped-down construction: just ensure the no-arg constructor doesn't throw.
        DesktopHttpClient c = new DesktopHttpClient();
        assertThat(c.getPersistentSettings()).isNotNull();
    }

    @Test
    void responseHeadersAreReturnedAsFirstValue() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200)
                .addHeader("X-Foo", "first")
                .addHeader("X-Foo", "second"));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        // JDK HttpClient lowercases header names in HttpHeaders.map(); accept either casing
        String value = resp.getHeaders().getOrDefault("X-Foo",
                resp.getHeaders().getOrDefault("x-foo", null));
        assertThat(value).isEqualTo("first");
    }
}
