package io.github.ndsev.zswag.desktop;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpResponse;
import io.github.ndsev.zswag.api.IHttpClient;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.util.Collections;
import java.util.LinkedHashMap;

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
        OAuth2Handler handler = new OAuth2Handler(stub);
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
        OAuth2Handler handler = new OAuth2Handler(stub);
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
        OAuth2Handler handler = new OAuth2Handler(stub);
        HttpConfig.OAuth2 oauth = HttpConfig.OAuth2.builder()
                .clientId("cid").clientSecret("csec").build();
        assertThatThrownBy(() -> handler.getAccessToken(oauth, "https://idp.example/token", "https://idp.example/token", Collections.emptyList()))
                .isInstanceOf(HttpException.class)
                .hasMessageContaining("invalid_client");
    }
}
