package io.github.ndsev.zswag.android;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpRequest;
import io.github.ndsev.zswag.api.HttpResponse;
import io.github.ndsev.zswag.api.HttpSettings;
import io.github.ndsev.zswag.api.IKeychain;
import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import okhttp3.mockwebserver.RecordedRequest;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.util.Collections;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Unit tests for {@link AndroidHttpClient}. AndroidHttpClient is a pure-Java
 * class (uses only OkHttp + java.net + javax.net.ssl, no android.* refs)
 * so we test it on plain JUnit + MockWebServer rather than Robolectric.
 * Exercises method dispatch, header / cookie / query / basic-auth merging,
 * per-request precedence, and the persistent-settings scope match. Mirrors
 * {@code JvmHttpClientTest}.
 */
public class AndroidHttpClientTest {

    private static final IKeychain THROWING_KC = (s, u) -> {
        throw new IllegalStateException("Keychain not used in this test");
    };

    private MockWebServer server;

    @BeforeEach
    public void start() throws IOException {
        server = new MockWebServer();
        server.start();
    }

    @AfterEach
    public void stop() throws IOException {
        server.shutdown();
    }

    private AndroidHttpClient newClient() {
        return new AndroidHttpClient(HttpSettings.empty(), THROWING_KC);
    }

    @Test
    public void getRequestSendsRequestAndReturnsResponse() throws Exception {
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
    public void postWithBodySendsBytes() throws Exception {
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
    public void postWithoutBodySendsEmpty() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(204));
        HttpRequest req = HttpRequest.builder().method("POST").url(server.url("/p").toString()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(204);
    }

    @Test
    public void putRequestSupported() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("PUT").url(server.url("/p").toString())
                .body("body".getBytes()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(200);
        assertThat(server.takeRequest().getMethod()).isEqualTo("PUT");
    }

    @Test
    public void deleteRequestSupportedWithoutBody() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(204));
        HttpRequest req = HttpRequest.builder().method("DELETE").url(server.url("/p").toString()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        assertThat(resp.getStatusCode()).isEqualTo(204);
        assertThat(server.takeRequest().getMethod()).isEqualTo("DELETE");
    }

    @Test
    public void deleteRequestSupportedWithBody() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(204));
        HttpRequest req = HttpRequest.builder().method("DELETE").url(server.url("/p").toString())
                .body("payload".getBytes()).build();
        newClient().execute(req, HttpConfig.empty());
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getMethod()).isEqualTo("DELETE");
        assertThat(recorded.getBody().readUtf8()).isEqualTo("payload");
    }

    @Test
    public void unsupportedHttpMethodThrows() {
        HttpRequest req = HttpRequest.builder().method("PATCH").url(server.url("/x").toString()).build();
        assertThatThrownBy(() -> newClient().execute(req, HttpConfig.empty()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("Unsupported HTTP method");
    }

    @Test
    public void perRequestHeadersTakePrecedenceOverAdhocHeaders() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString())
                .header("Authorization", "Bearer per-request").build();
        HttpConfig adhoc = HttpConfig.builder().bearerToken("from-adhoc").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getHeaders().values("Authorization")).containsExactly("Bearer per-request");
    }

    @Test
    public void cookiesFromConfigAreSent() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder().cookie("a", "1").cookie("b", "2").build();
        newClient().execute(req, adhoc);
        RecordedRequest recorded = server.takeRequest();
        assertThat(recorded.getHeader("Cookie")).contains("a=1").contains("b=2");
    }

    @Test
    public void perRequestCookieHeaderSuppressesConfigCookies() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString())
                .header("Cookie", "explicit=yes").build();
        HttpConfig adhoc = HttpConfig.builder().cookie("a", "1").build();
        newClient().execute(req, adhoc);
        assertThat(server.takeRequest().getHeader("Cookie")).isEqualTo("explicit=yes");
    }

    @Test
    public void basicAuthFromConfigInjectsAuthorizationHeader() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder().basicAuth("alice", "secret").build();
        newClient().execute(req, adhoc);
        // base64("alice:secret") = "YWxpY2U6c2VjcmV0"
        assertThat(server.takeRequest().getHeader("Authorization")).isEqualTo("Basic YWxpY2U6c2VjcmV0");
    }

    @Test
    public void basicAuthSuppressedWhenAuthorizationAlreadyOnRequest() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString())
                .header("Authorization", "Bearer prebaked").build();
        HttpConfig adhoc = HttpConfig.builder().basicAuth("alice", "secret").build();
        newClient().execute(req, adhoc);
        assertThat(server.takeRequest().getHeaders().values("Authorization")).containsExactly("Bearer prebaked");
    }

    @Test
    public void basicAuthSuppressedWhenAuthorizationInConfigHeaders() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder()
                .header("authorization", "Bearer x")
                .basicAuth("alice", "secret")
                .build();
        newClient().execute(req, adhoc);
        assertThat(server.takeRequest().getHeader("Authorization")).contains("Bearer x");
    }

    @Test
    public void adhocHeadersFromConfigAreSent() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder()
                .addHeader("X-Multi", "v1")
                .addHeader("X-Multi", "v2")
                .build();
        newClient().execute(req, adhoc);
        assertThat(server.takeRequest().getHeaders().values("X-Multi")).containsExactly("v1", "v2");
    }

    @Test
    public void queryParametersAreAppendedToUrl() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpConfig adhoc = HttpConfig.builder()
                .addQuery("a", "1")
                .addQuery("a", "2")
                .addQuery("b", "x y")
                .build();
        newClient().execute(req, adhoc);
        String path = server.takeRequest().getPath();
        assertThat(path).contains("a=1").contains("a=2").contains("b=x+y");
    }

    @Test
    public void queryParamsAppendedWithExistingQueryString() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p?fixed=yes").toString()).build();
        HttpConfig adhoc = HttpConfig.builder().query("extra", "1").build();
        newClient().execute(req, adhoc);
        String path = server.takeRequest().getPath();
        assertThat(path).contains("fixed=yes").contains("extra=1");
    }

    @Test
    public void persistentSettingsAreScopeMergedAndAvailableForGetter() {
        HttpConfig wildcard = HttpConfig.builder()
                .scope("*", HttpSettings.compileScope("*"))
                .header("X-Default", "global")
                .build();
        HttpSettings persistent = new HttpSettings(Collections.singletonList(wildcard));
        AndroidHttpClient client = new AndroidHttpClient(persistent, THROWING_KC);
        assertThat(client.getPersistentSettings()).isSameAs(persistent);
    }

    @Test
    public void persistentScopeMatchesAndAddsHeaders() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200));
        String url = server.url("/p").toString();
        HttpConfig wildcard = HttpConfig.builder()
                .scope("*", HttpSettings.compileScope("*"))
                .header("X-Default", "yes")
                .build();
        AndroidHttpClient client = new AndroidHttpClient(
                new HttpSettings(Collections.singletonList(wildcard)), THROWING_KC);
        HttpRequest req = HttpRequest.builder().method("GET").url(url).build();
        client.execute(req, HttpConfig.empty());
        assertThat(server.takeRequest().getHeader("X-Default")).isEqualTo("yes");
    }

    @Test
    public void connectionFailureSurfacesAsHttpException() {
        HttpRequest req = HttpRequest.builder().method("GET").url("http://127.0.0.1:1/x").build();
        assertThatThrownBy(() -> newClient().execute(req, HttpConfig.empty()))
                .isInstanceOf(HttpException.class);
    }

    @Test
    public void responseHeadersAreReturnedAsFirstValue() throws Exception {
        server.enqueue(new MockResponse().setResponseCode(200)
                .addHeader("X-Foo", "first")
                .addHeader("X-Foo", "second"));
        HttpRequest req = HttpRequest.builder().method("GET").url(server.url("/p").toString()).build();
        HttpResponse resp = newClient().execute(req, HttpConfig.empty());
        // OkHttp normalises header casing differently than the JDK; accept either form.
        String value = resp.getHeaders().getOrDefault("X-Foo",
                resp.getHeaders().getOrDefault("x-foo", null));
        assertThat(value).isEqualTo("first");
    }
}
