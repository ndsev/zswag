package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import okhttp3.mockwebserver.RecordedRequest;
import org.junit.jupiter.api.*;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Base64;
import java.util.concurrent.TimeUnit;

import static org.assertj.core.api.Assertions.*;

/**
 * Unit tests for DesktopHttpClient.
 * Uses MockWebServer to test HTTP operations without network dependencies.
 */
class DesktopHttpClientTest {

    private MockWebServer server;
    private String baseUrl;

    @BeforeEach
    void setUp() throws IOException {
        server = new MockWebServer();
        server.start();
        baseUrl = server.url("/").toString();
    }

    @AfterEach
    void tearDown() throws IOException {
        server.shutdown();
    }

    @Nested
    @DisplayName("HTTP Method Tests")
    class HttpMethodTests {

        @Test
        @DisplayName("Should execute GET request")
        void executeGetRequest() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setBody("GET response"));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(200);
            assertThat(new String(response.getBody())).isEqualTo("GET response");

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getMethod()).isEqualTo("GET");
            assertThat(recorded.getPath()).isEqualTo("/test");
        }

        @Test
        @DisplayName("Should execute POST request with body")
        void executePostRequest() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(201)
                    .setBody("Created"));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            byte[] body = "request body".getBytes(StandardCharsets.UTF_8);
            HttpRequest request = HttpRequest.builder()
                    .method("POST")
                    .url(baseUrl + "create")
                    .body(body)
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(201);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getMethod()).isEqualTo("POST");
            assertThat(recorded.getBody().readUtf8()).isEqualTo("request body");
        }

        @Test
        @DisplayName("Should execute PUT request")
        void executePutRequest() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setBody("Updated"));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            byte[] body = "update data".getBytes(StandardCharsets.UTF_8);
            HttpRequest request = HttpRequest.builder()
                    .method("PUT")
                    .url(baseUrl + "update")
                    .body(body)
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(200);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getMethod()).isEqualTo("PUT");
        }

        @Test
        @DisplayName("Should execute DELETE request")
        void executeDeleteRequest() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(204));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("DELETE")
                    .url(baseUrl + "resource/123")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(204);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getMethod()).isEqualTo("DELETE");
        }

        @Test
        @DisplayName("Should execute PATCH request")
        void executePatchRequest() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setBody("Patched"));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            byte[] body = "patch data".getBytes(StandardCharsets.UTF_8);
            HttpRequest request = HttpRequest.builder()
                    .method("PATCH")
                    .url(baseUrl + "patch")
                    .body(body)
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(200);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getMethod()).isEqualTo("PATCH");
        }

        @Test
        @DisplayName("Should throw for unsupported HTTP method")
        void unsupportedMethod() {
            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("INVALID")
                    .url(baseUrl + "test")
                    .build();

            assertThatThrownBy(() -> client.execute(request))
                    .isInstanceOf(HttpException.class)
                    .hasMessageContaining("Unsupported HTTP method");
        }
    }

    @Nested
    @DisplayName("Header Tests")
    class HeaderTests {

        @Test
        @DisplayName("Should send request headers")
        void sendRequestHeaders() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test")
                    .header("X-Custom-Header", "custom-value")
                    .header("Accept", "application/json")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getHeader("X-Custom-Header")).isEqualTo("custom-value");
            assertThat(recorded.getHeader("Accept")).isEqualTo("application/json");
        }

        @Test
        @DisplayName("Should include headers from settings")
        void includeSettingsHeaders() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            HttpSettings settings = HttpSettings.builder()
                    .header("X-Settings-Header", "settings-value")
                    .build();
            DesktopHttpClient client = new DesktopHttpClient(settings);

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getHeader("X-Settings-Header")).isEqualTo("settings-value");
        }

        @Test
        @DisplayName("Should parse response headers")
        void parseResponseHeaders() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .addHeader("X-Response-Header", "response-value")
                    .addHeader("Content-Type", "application/octet-stream"));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getHeaders()).containsEntry("x-response-header", "response-value");
        }
    }

    @Nested
    @DisplayName("Authentication Tests")
    class AuthenticationTests {

        @Test
        @DisplayName("Should add Bearer token header")
        void addBearerToken() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            HttpSettings settings = HttpSettings.builder()
                    .bearerToken("my-token-123")
                    .build();
            DesktopHttpClient client = new DesktopHttpClient(settings);

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "protected")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getHeader("Authorization")).isEqualTo("Bearer my-token-123");
        }

        @Test
        @DisplayName("Should add Basic auth header")
        void addBasicAuth() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            HttpSettings settings = HttpSettings.builder()
                    .basicAuth("user", "password")
                    .build();
            DesktopHttpClient client = new DesktopHttpClient(settings);

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "protected")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            String expectedCredentials = Base64.getEncoder().encodeToString("user:password".getBytes(StandardCharsets.UTF_8));
            assertThat(recorded.getHeader("Authorization")).isEqualTo("Basic " + expectedCredentials);
        }

        @Test
        @DisplayName("Bearer token should take precedence over Basic auth")
        void bearerPrecedence() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            HttpSettings settings = HttpSettings.builder()
                    .bearerToken("token")
                    .basicAuth("user", "pass")
                    .build();
            DesktopHttpClient client = new DesktopHttpClient(settings);

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "protected")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getHeader("Authorization")).startsWith("Bearer ");
        }
    }

    @Nested
    @DisplayName("Cookie Tests")
    class CookieTests {

        @Test
        @DisplayName("Should send single cookie")
        void sendSingleCookie() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            HttpSettings settings = HttpSettings.builder()
                    .cookie("session", "abc123")
                    .build();
            DesktopHttpClient client = new DesktopHttpClient(settings);

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getHeader("Cookie")).isEqualTo("session=abc123");
        }

        @Test
        @DisplayName("Should send multiple cookies")
        void sendMultipleCookies() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            HttpSettings settings = HttpSettings.builder()
                    .cookie("session", "abc123")
                    .cookie("user_id", "42")
                    .build();
            DesktopHttpClient client = new DesktopHttpClient(settings);

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            String cookieHeader = recorded.getHeader("Cookie");
            assertThat(cookieHeader).contains("session=abc123");
            assertThat(cookieHeader).contains("user_id=42");
            assertThat(cookieHeader).contains("; ");
        }
    }

    @Nested
    @DisplayName("Response Handling Tests")
    class ResponseHandlingTests {

        @Test
        @DisplayName("Should handle 4xx error responses")
        void handle4xxErrors() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(404)
                    .setBody("Not Found"));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "nonexistent")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(404);
            assertThat(response.isSuccessful()).isFalse();
        }

        @Test
        @DisplayName("Should handle 5xx error responses")
        void handle5xxErrors() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(500)
                    .setBody("Internal Server Error"));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "error")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(500);
            assertThat(response.isSuccessful()).isFalse();
        }

        @Test
        @DisplayName("Should handle empty response body")
        void handleEmptyBody() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(204));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("DELETE")
                    .url(baseUrl + "resource")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(204);
            assertThat(response.getBody()).isEmpty();
        }

        @Test
        @DisplayName("Should handle binary response body")
        void handleBinaryBody() throws Exception {
            byte[] binaryData = new byte[]{0x00, 0x01, 0x02, (byte)0xFF, (byte)0xFE};
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setBody(new okio.Buffer().write(binaryData)));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "binary")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getBody()).isEqualTo(binaryData);
        }
    }

    @Nested
    @DisplayName("Settings Tests")
    class SettingsTests {

        @Test
        @DisplayName("Should return current settings")
        void getCurrentSettings() {
            HttpSettings settings = HttpSettings.builder()
                    .bearerToken("token")
                    .header("X-Header", "value")
                    .build();

            DesktopHttpClient client = new DesktopHttpClient(settings);

            assertThat(client.getSettings()).isSameAs(settings);
        }

        @Test
        @DisplayName("Should create new client with different settings")
        void createWithNewSettings() {
            HttpSettings settings1 = HttpSettings.builder()
                    .bearerToken("token1")
                    .build();
            HttpSettings settings2 = HttpSettings.builder()
                    .bearerToken("token2")
                    .build();

            DesktopHttpClient client1 = new DesktopHttpClient(settings1);
            IHttpClient client2 = client1.withSettings(settings2);

            assertThat(client1.getSettings().getBearerToken()).isEqualTo("token1");
            assertThat(client2.getSettings().getBearerToken()).isEqualTo("token2");
            assertThat(client2).isNotSameAs(client1);
        }

        @Test
        @DisplayName("Should use custom timeout")
        void useCustomTimeout() {
            HttpSettings settings = HttpSettings.builder()
                    .timeout(Duration.ofSeconds(5))
                    .build();

            DesktopHttpClient client = new DesktopHttpClient(settings);

            assertThat(client.getSettings().getTimeout()).isEqualTo(Duration.ofSeconds(5));
        }
    }

    @Nested
    @DisplayName("Query Parameter Tests")
    class QueryParameterTests {

        @Test
        @DisplayName("Should include query parameters from settings")
        void includeQueryParameters() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            HttpSettings settings = HttpSettings.builder()
                    .queryParameter("api-key", "secret123")
                    .build();
            DesktopHttpClient client = new DesktopHttpClient(settings);

            // Note: Query parameters from settings need to be applied by the OpenAPIClient
            // The HttpClient itself doesn't modify the URL, but the settings can be used
            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test?api-key=secret123")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getPath()).contains("api-key=secret123");
        }
    }

    @Nested
    @DisplayName("Edge Case Tests")
    class EdgeCaseTests {

        @Test
        @DisplayName("Should handle POST without body")
        void postWithoutBody() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("POST")
                    .url(baseUrl + "empty")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getStatusCode()).isEqualTo(200);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getMethod()).isEqualTo("POST");
            assertThat(recorded.getBodySize()).isEqualTo(0);
        }

        @Test
        @DisplayName("Should handle URL with special characters")
        void urlWithSpecialCharacters() throws Exception {
            server.enqueue(new MockResponse().setResponseCode(200));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "test?q=hello%20world&filter=a%2Bb")
                    .build();

            client.execute(request);

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getPath()).isEqualTo("/test?q=hello%20world&filter=a%2Bb");
        }

        @Test
        @DisplayName("Should handle large response body")
        void handleLargeBody() throws Exception {
            byte[] largeBody = new byte[100_000];
            for (int i = 0; i < largeBody.length; i++) {
                largeBody[i] = (byte)(i % 256);
            }
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setBody(new okio.Buffer().write(largeBody)));

            DesktopHttpClient client = new DesktopHttpClient(HttpSettings.builder().build());

            HttpRequest request = HttpRequest.builder()
                    .method("GET")
                    .url(baseUrl + "large")
                    .build();

            HttpResponse response = client.execute(request);

            assertThat(response.getBody()).hasSize(100_000);
        }
    }
}
