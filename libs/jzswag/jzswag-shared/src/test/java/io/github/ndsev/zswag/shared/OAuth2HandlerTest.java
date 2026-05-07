package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpRequest;
import io.github.ndsev.zswag.api.HttpResponse;
import io.github.ndsev.zswag.api.IHttpClient;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Regression tests for {@link OAuth2Handler} pinning behaviours that production
 * deployments depend on but the broader test suite did not previously cover.
 */
class OAuth2HandlerTest {

    @BeforeEach
    void clearTokenCache() {
        OAuth2Handler.clearAllCachedTokens();
    }

    @Test
    void requestTokenThrowsDescriptiveErrorOnEmpty2xxBody() {
        // Regression: previously `new String(response.getBody(), UTF-8)` NPE'd if a
        // misbehaving token endpoint returned 200 with an empty/null body.
        IHttpClient stub = (request, adhoc) -> new HttpResponse(200, null, new LinkedHashMap<>(), null);
        OAuth2Handler handler = new OAuth2Handler(stub, (s,u) -> "test-keychain-secret");
        HttpConfig.OAuth2 oauth = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").build();
        assertThatThrownBy(() -> handler.getAccessToken(oauth, "https://idp.example/token", "https://idp.example/token", Collections.emptyList()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("empty body");
    }

    @Test
    void requestTokenThrowsWhenAccessTokenMissingFromResponse() {
        IHttpClient stub = (request, adhoc) -> new HttpResponse(
                200, null, new LinkedHashMap<>(),
                "{\"token_type\":\"bearer\"}".getBytes());
        OAuth2Handler handler = new OAuth2Handler(stub, (s,u) -> "test-keychain-secret");
        HttpConfig.OAuth2 oauth = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").build();
        assertThatThrownBy(() -> handler.getAccessToken(oauth, "https://idp.example/token", "https://idp.example/token", Collections.emptyList()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("access_token");
    }

    @Test
    void requestTokenSurfacesNon2xxWithBodyInMessage() {
        IHttpClient stub = (request, adhoc) -> new HttpResponse(
                401, null, new LinkedHashMap<>(),
                "{\"error\":\"invalid_client\"}".getBytes());
        OAuth2Handler handler = new OAuth2Handler(stub, (s,u) -> "test-keychain-secret");
        HttpConfig.OAuth2 oauth = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").build();
        assertThatThrownBy(() -> handler.getAccessToken(oauth, "https://idp.example/token", "https://idp.example/token", Collections.emptyList()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("invalid_client");
    }

    @Test
    void cachedTokenReusedOnSecondCall() throws Exception {
        // First call mints; second call hits the cache and does not call the IdP again.
        AtomicInteger callCount = new AtomicInteger(0);
        IHttpClient stub = (request, adhoc) -> {
            callCount.incrementAndGet();
            return jsonResponse(200, "{\"access_token\":\"tok-1\",\"expires_in\":3600}");
        };
        OAuth2Handler handler = new OAuth2Handler(stub, (s, u) -> "");
        HttpConfig.OAuth2 oauth = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").build();

        String t1 = handler.getAccessToken(oauth, "https://idp/token", "https://idp/token", Collections.emptyList());
        String t2 = handler.getAccessToken(oauth, "https://idp/token", "https://idp/token", Collections.emptyList());

        assertThat(t1).isEqualTo("tok-1");
        assertThat(t2).isEqualTo("tok-1");
        assertThat(callCount.get()).as("token endpoint hit only once").isEqualTo(1);
    }

    @Test
    void distinctTokenKeysDoNotCollideInCache() throws Exception {
        // Two clients with different (clientId) keys should each have their own cache entry.
        IHttpClient stub = (request, adhoc) -> {
            String body = new String(request.getBody(), StandardCharsets.UTF_8);
            String tokenForClient = body.contains("&audience=tenant-A") ? "tok-A" : "tok-B";
            return jsonResponse(200, "{\"access_token\":\"" + tokenForClient + "\",\"expires_in\":3600}");
        };
        OAuth2Handler handler = new OAuth2Handler(stub, (s, u) -> "");
        HttpConfig.OAuth2 a = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").audience("tenant-A").build();
        HttpConfig.OAuth2 b = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").audience("tenant-B").build();

        String tA = handler.getAccessToken(a, "https://idp/token", "https://idp/token", Collections.emptyList());
        String tB = handler.getAccessToken(b, "https://idp/token", "https://idp/token", Collections.emptyList());

        assertThat(tA).isEqualTo("tok-A");
        assertThat(tB).isEqualTo("tok-B");
    }

    @Test
    void expiredTokenWithRefreshTokenTriggersRefresh() throws Exception {
        // Mint a token that has a refresh_token, expire it manually, then call again.
        // The handler should issue a refresh_token grant (not a fresh client_credentials mint).
        List<String> grantTypes = new java.util.ArrayList<>();
        IHttpClient stub = (request, adhoc) -> {
            String body = new String(request.getBody(), StandardCharsets.UTF_8);
            String grantType = body.split("&")[0].split("=")[1];
            grantTypes.add(grantType);
            String accessToken = "refresh_token".equals(grantType) ? "tok-refreshed" : "tok-1";
            return jsonResponse(200, "{\"access_token\":\"" + accessToken
                    + "\",\"refresh_token\":\"rtok\",\"expires_in\":3600}");
        };
        OAuth2Handler handler = new OAuth2Handler(stub, (s, u) -> "");
        HttpConfig.OAuth2 oauth = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").build();

        String t1 = handler.getAccessToken(oauth, "https://idp/token", "https://idp/token", Collections.emptyList());
        expireCachedToken("https://idp/token", "cid", "", "");
        String t2 = handler.getAccessToken(oauth, "https://idp/token", "https://idp/token", Collections.emptyList());

        assertThat(t1).isEqualTo("tok-1");
        assertThat(t2).isEqualTo("tok-refreshed");
        assertThat(grantTypes).containsExactly("client_credentials", "refresh_token");
    }

    @Test
    void refreshFailureFallsBackToFreshMint() throws Exception {
        // When the refresh_token grant fails, the handler should retry with client_credentials.
        List<String> grantTypes = new java.util.ArrayList<>();
        IHttpClient stub = (request, adhoc) -> {
            String body = new String(request.getBody(), StandardCharsets.UTF_8);
            String grantType = body.split("&")[0].split("=")[1];
            grantTypes.add(grantType);
            if ("refresh_token".equals(grantType)) {
                // Simulate refresh failure (token revoked / IdP rotated keys).
                return jsonResponse(401, "{\"error\":\"invalid_grant\"}");
            }
            return jsonResponse(200, "{\"access_token\":\"fresh-tok\",\"refresh_token\":\"rtok\",\"expires_in\":3600}");
        };
        OAuth2Handler handler = new OAuth2Handler(stub, (s, u) -> "");
        HttpConfig.OAuth2 oauth = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").build();

        // First mint succeeds and caches a refresh token.
        handler.getAccessToken(oauth, "https://idp/token", "https://idp/token", Collections.emptyList());
        expireCachedToken("https://idp/token", "cid", "", "");
        // Second call: refresh fails → fresh mint succeeds.
        String t2 = handler.getAccessToken(oauth, "https://idp/token", "https://idp/token", Collections.emptyList());

        assertThat(t2).isEqualTo("fresh-tok");
        assertThat(grantTypes).as("ordering: initial mint, failed refresh, fallback mint")
                .containsExactly("client_credentials", "refresh_token", "client_credentials");
    }

    // ------------------------------------------------------------------------
    // Test helpers — reach into OAuth2Handler's private cache to manipulate
    // expiry without sleeping. The alternative (Thread.sleep) is flaky and slow.
    // ------------------------------------------------------------------------

    private static HttpResponse jsonResponse(int status, String json) {
        return new HttpResponse(status, null, new LinkedHashMap<>(), json.getBytes(StandardCharsets.UTF_8));
    }

    @SuppressWarnings("unchecked")
    private static void expireCachedToken(String tokenUrl, String clientId, String audience, String scopeKey)
            throws Exception {
        Field cacheField = OAuth2Handler.class.getDeclaredField("CACHE");
        cacheField.setAccessible(true);
        java.util.Map<Object, Object> cache = (java.util.Map<Object, Object>) cacheField.get(null);
        Class<?> tokenKeyClass = Class.forName(OAuth2Handler.class.getName() + "$TokenKey");
        java.lang.reflect.Constructor<?> tkCtor = tokenKeyClass.getDeclaredConstructor(
                String.class, String.class, String.class, String.class);
        tkCtor.setAccessible(true);
        Object key = tkCtor.newInstance(tokenUrl, clientId, audience, scopeKey);
        Object minted = cache.get(key);
        if (minted == null) {
            throw new IllegalStateException("No cached token for key " + key);
        }
        Field expiresAt = minted.getClass().getDeclaredField("expiresAt");
        expiresAt.setAccessible(true);
        expiresAt.set(minted, Instant.now().minusSeconds(60));
    }
}
