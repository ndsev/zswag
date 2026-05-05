package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;
import okhttp3.mockwebserver.RecordedRequest;
import org.junit.jupiter.api.*;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import java.util.concurrent.*;

import static org.assertj.core.api.Assertions.*;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.*;

/**
 * Unit tests for OAuth2Handler.
 * Tests token acquisition, caching, and refresh behavior.
 */
@ExtendWith(MockitoExtension.class)
class OAuth2HandlerTest {

    private MockWebServer server;
    private String tokenEndpoint;

    @BeforeEach
    void setUp() throws IOException {
        server = new MockWebServer();
        server.start();
        tokenEndpoint = server.url("/oauth/token").toString();
    }

    @AfterEach
    void tearDown() throws IOException {
        server.shutdown();
    }

    @Nested
    @DisplayName("Token Acquisition Tests")
    class TokenAcquisitionTests {

        @Test
        @DisplayName("Should acquire access token")
        void acquireAccessToken() throws Exception {
            String tokenResponse = "{\"access_token\":\"test-token-123\",\"token_type\":\"Bearer\",\"expires_in\":3600}";
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody(tokenResponse));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client-id", "client-secret", null, httpClient);

            String token = handler.getAccessToken();

            assertThat(token).isEqualTo("test-token-123");

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            assertThat(recorded.getMethod()).isEqualTo("POST");
            assertThat(recorded.getHeader("Content-Type")).isEqualTo("application/x-www-form-urlencoded");
        }

        @Test
        @DisplayName("Should send Basic Auth header with client credentials")
        void sendBasicAuthHeader() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "my-client", "my-secret", null, httpClient);

            handler.getAccessToken();

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            String expectedAuth = Base64.getEncoder().encodeToString("my-client:my-secret".getBytes(StandardCharsets.UTF_8));
            assertThat(recorded.getHeader("Authorization")).isEqualTo("Basic " + expectedAuth);
        }

        @Test
        @DisplayName("Should send grant_type=client_credentials")
        void sendGrantType() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            handler.getAccessToken();

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            String body = recorded.getBody().readUtf8();
            assertThat(body).contains("grant_type=client_credentials");
        }

        @Test
        @DisplayName("Should include scope when provided")
        void includeScope() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", "read write", httpClient);

            handler.getAccessToken();

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            String body = recorded.getBody().readUtf8();
            // URL form encoding uses + for spaces (per application/x-www-form-urlencoded)
            assertThat(body).contains("scope=read+write");
        }

        @Test
        @DisplayName("Should not include scope when null")
        void noScopeWhenNull() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            handler.getAccessToken();

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            String body = recorded.getBody().readUtf8();
            assertThat(body).doesNotContain("scope=");
        }
    }

    @Nested
    @DisplayName("Token Caching Tests")
    class TokenCachingTests {

        @Test
        @DisplayName("Should cache token and reuse it")
        void cacheToken() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"cached-token\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            // First call - should hit server
            String token1 = handler.getAccessToken();
            // Second call - should use cache
            String token2 = handler.getAccessToken();
            // Third call - should use cache
            String token3 = handler.getAccessToken();

            assertThat(token1).isEqualTo("cached-token");
            assertThat(token2).isEqualTo("cached-token");
            assertThat(token3).isEqualTo("cached-token");

            // Only one request should have been made
            assertThat(server.getRequestCount()).isEqualTo(1);
        }

        @Test
        @DisplayName("Should clear token cache")
        void clearTokenCache() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token-1\",\"expires_in\":3600}"));
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token-2\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            String token1 = handler.getAccessToken();
            handler.clearToken();
            String token2 = handler.getAccessToken();

            assertThat(token1).isEqualTo("token-1");
            assertThat(token2).isEqualTo("token-2");
            assertThat(server.getRequestCount()).isEqualTo(2);
        }

        @Test
        @DisplayName("Should use default expiry if not provided")
        void defaultExpiry() throws Exception {
            // Response without expires_in
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token\",\"token_type\":\"Bearer\"}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            String token = handler.getAccessToken();

            assertThat(token).isEqualTo("token");
            // Token should be cached (default expiry is 3600s)
            assertThat(server.getRequestCount()).isEqualTo(1);
            handler.getAccessToken();
            assertThat(server.getRequestCount()).isEqualTo(1);
        }
    }

    @Nested
    @DisplayName("Error Handling Tests")
    class ErrorHandlingTests {

        @Test
        @DisplayName("Should throw on 401 unauthorized")
        void throwOn401() {
            server.enqueue(new MockResponse()
                    .setResponseCode(401)
                    .setBody("{\"error\":\"invalid_client\"}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "bad-client", "bad-secret", null, httpClient);

            assertThatThrownBy(handler::getAccessToken)
                    .isInstanceOf(HttpException.class)
                    .hasMessageContaining("OAuth2 token request failed");
        }

        @Test
        @DisplayName("Should throw on 400 bad request")
        void throwOn400() {
            server.enqueue(new MockResponse()
                    .setResponseCode(400)
                    .setBody("{\"error\":\"invalid_grant\"}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            assertThatThrownBy(handler::getAccessToken)
                    .isInstanceOf(HttpException.class)
                    .hasMessageContaining("OAuth2 token request failed");
        }

        @Test
        @DisplayName("Should throw on 500 server error")
        void throwOn500() {
            server.enqueue(new MockResponse()
                    .setResponseCode(500)
                    .setBody("Internal Server Error"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            assertThatThrownBy(handler::getAccessToken)
                    .isInstanceOf(HttpException.class);
        }
    }

    @Nested
    @DisplayName("Thread Safety Tests")
    class ThreadSafetyTests {

        @Test
        @DisplayName("Should handle concurrent token requests")
        void concurrentRequests() throws Exception {
            // Enqueue response - should only be called once
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"concurrent-token\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", null, httpClient);

            // Create multiple threads all requesting tokens
            int threadCount = 10;
            ExecutorService executor = Executors.newFixedThreadPool(threadCount);
            CountDownLatch startLatch = new CountDownLatch(1);
            CountDownLatch doneLatch = new CountDownLatch(threadCount);
            ConcurrentLinkedQueue<String> tokens = new ConcurrentLinkedQueue<>();
            ConcurrentLinkedQueue<Exception> errors = new ConcurrentLinkedQueue<>();

            for (int i = 0; i < threadCount; i++) {
                executor.submit(() -> {
                    try {
                        startLatch.await();
                        String token = handler.getAccessToken();
                        tokens.add(token);
                    } catch (Exception e) {
                        errors.add(e);
                    } finally {
                        doneLatch.countDown();
                    }
                });
            }

            // Start all threads simultaneously
            startLatch.countDown();
            doneLatch.await(5, TimeUnit.SECONDS);
            executor.shutdown();

            assertThat(errors).isEmpty();
            assertThat(tokens).hasSize(threadCount);
            // All tokens should be the same
            assertThat(tokens).allMatch(t -> t.equals("concurrent-token"));
            // Only one HTTP request should have been made
            assertThat(server.getRequestCount()).isEqualTo(1);
        }
    }

    @Nested
    @DisplayName("URL Encoding Tests")
    class UrlEncodingTests {

        @Test
        @DisplayName("Should URL encode special characters in scope")
        void encodeSpecialCharsInScope() throws Exception {
            server.enqueue(new MockResponse()
                    .setResponseCode(200)
                    .setHeader("Content-Type", "application/json")
                    .setBody("{\"access_token\":\"token\",\"expires_in\":3600}"));

            IHttpClient httpClient = new DesktopHttpClient(HttpSettings.builder().build());
            OAuth2Handler handler = new OAuth2Handler(tokenEndpoint, "client", "secret", "scope:with&special", httpClient);

            handler.getAccessToken();

            RecordedRequest recorded = server.takeRequest(1, TimeUnit.SECONDS);
            String body = recorded.getBody().readUtf8();
            // Special chars should be URL encoded (: becomes %3A, & becomes %26)
            assertThat(body).contains("scope=scope%3Awith%26special");
        }
    }

    @Nested
    @DisplayName("Integration Tests with Mocked Client")
    class MockedClientTests {

        @Mock
        private IHttpClient mockHttpClient;

        @Test
        @DisplayName("Should use provided HTTP client")
        void useProvidedClient() throws Exception {
            String tokenResponse = "{\"access_token\":\"mocked-token\",\"expires_in\":3600}";
            HttpResponse mockResponse = new HttpResponse(200, "OK", null, tokenResponse.getBytes(StandardCharsets.UTF_8));

            when(mockHttpClient.execute(any())).thenReturn(mockResponse);

            OAuth2Handler handler = new OAuth2Handler("https://auth.example.com/token", "client", "secret", null, mockHttpClient);

            String token = handler.getAccessToken();

            assertThat(token).isEqualTo("mocked-token");
            verify(mockHttpClient, times(1)).execute(any(HttpRequest.class));
        }

        @Test
        @DisplayName("Should verify request structure")
        void verifyRequestStructure() throws Exception {
            String tokenResponse = "{\"access_token\":\"token\",\"expires_in\":3600}";
            HttpResponse mockResponse = new HttpResponse(200, "OK", null, tokenResponse.getBytes(StandardCharsets.UTF_8));

            when(mockHttpClient.execute(any())).thenAnswer(invocation -> {
                HttpRequest request = invocation.getArgument(0);

                // Verify request structure
                assertThat(request.getMethod()).isEqualTo("POST");
                assertThat(request.getUrl()).isEqualTo("https://auth.example.com/token");
                assertThat(request.getHeaders().get("Content-Type")).isEqualTo("application/x-www-form-urlencoded");
                assertThat(request.getHeaders().get("Authorization")).startsWith("Basic ");

                String body = new String(request.getBody(), StandardCharsets.UTF_8);
                assertThat(body).contains("grant_type=client_credentials");

                return mockResponse;
            });

            OAuth2Handler handler = new OAuth2Handler("https://auth.example.com/token", "client", "secret", null, mockHttpClient);
            handler.getAccessToken();
        }
    }
}
