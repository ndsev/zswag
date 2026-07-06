package io.github.ndsev.zswag.api;

import org.junit.jupiter.api.Test;

import java.time.Duration;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.regex.Pattern;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class HttpConfigTest {

    @Test
    void emptyConfigHasDefaults() {
        HttpConfig c = HttpConfig.empty();
        assertThat(c.getHeaders()).isEmpty();
        assertThat(c.getQuery()).isEmpty();
        assertThat(c.getCookies()).isEmpty();
        assertThat(c.getAuth()).isEmpty();
        assertThat(c.getProxy()).isEmpty();
        assertThat(c.getOAuth2()).isEmpty();
        assertThat(c.getApiKey()).isEmpty();
        assertThat(c.getScope()).isEmpty();
        assertThat(c.getUrlPattern()).isEmpty();
        assertThat(c.isSslStrict()).isTrue();
        assertThat(c.getTimeout()).isEqualTo(Duration.ofSeconds(60));
    }

    @Test
    void builderCollectsHeadersQueriesCookies() {
        HttpConfig c = HttpConfig.builder()
                .header("X-A", "v1")
                .addHeader("X-A", "v2")
                .query("q", "1")
                .addQuery("q", "2")
                .cookie("session", "abc")
                .build();
        assertThat(c.getHeaders().get("X-A")).containsExactly("v1", "v2");
        assertThat(c.getQuery().get("q")).containsExactly("1", "2");
        assertThat(c.getCookies()).containsEntry("session", "abc");
    }

    @Test
    void headerReplacesPreviousValueAddHeaderAccumulates() {
        HttpConfig c = HttpConfig.builder()
                .header("X", "first")
                .header("X", "second")  // header() should clear and replace
                .build();
        assertThat(c.getHeaders().get("X")).containsExactly("second");
    }

    @Test
    void queryReplacesPreviousValueAddQueryAccumulates() {
        HttpConfig c = HttpConfig.builder()
                .query("k", "first")
                .query("k", "second")  // query() should clear and replace
                .build();
        assertThat(c.getQuery().get("k")).containsExactly("second");
    }

    @Test
    void getHeaderReturnsFirstValue() {
        HttpConfig c = HttpConfig.builder().addHeader("X", "v1").addHeader("X", "v2").build();
        assertThat(c.getHeader("X")).contains("v1");
        assertThat(c.getHeader("Y")).isEmpty();
    }

    @Test
    void headersBulkBuilderAcceptsMap() {
        Map<String, String> bulk = new LinkedHashMap<>();
        bulk.put("A", "1");
        bulk.put("B", "2");
        HttpConfig c = HttpConfig.builder().headers(bulk).build();
        assertThat(c.getHeaders().get("A")).containsExactly("1");
        assertThat(c.getHeaders().get("B")).containsExactly("2");
    }

    @Test
    void cookiesBulkBuilderAcceptsMap() {
        Map<String, String> bulk = new LinkedHashMap<>();
        bulk.put("a", "1");
        bulk.put("b", "2");
        HttpConfig c = HttpConfig.builder().cookies(bulk).build();
        assertThat(c.getCookies()).containsEntry("a", "1").containsEntry("b", "2");
    }

    @Test
    void bearerTokenSetsAuthorizationHeader() {
        HttpConfig c = HttpConfig.builder().bearerToken("xyz").build();
        assertThat(c.getHeader("Authorization")).contains("Bearer xyz");
    }

    @Test
    void basicAuthFactoryFormsKeychainOrPassword() {
        HttpConfig.BasicAuthentication pwd = HttpConfig.BasicAuthentication.ofPassword("u", "p");
        assertThat(pwd.user).isEqualTo("u");
        assertThat(pwd.password).isEqualTo("p");
        assertThat(pwd.keychain).isEmpty();
        HttpConfig.BasicAuthentication kc = HttpConfig.BasicAuthentication.ofKeychain("u2", "svc");
        assertThat(kc.user).isEqualTo("u2");
        assertThat(kc.password).isEmpty();
        assertThat(kc.keychain).isEqualTo("svc");
    }

    @Test
    void proxyConstructorStoresAllFields() {
        HttpConfig.Proxy p = new HttpConfig.Proxy("127.0.0.1", 3128, "u", "pw", "kc");
        assertThat(p.host).isEqualTo("127.0.0.1");
        assertThat(p.port).isEqualTo(3128);
        assertThat(p.user).isEqualTo("u");
        assertThat(p.password).isEqualTo("pw");
        assertThat(p.keychain).isEqualTo("kc");
    }

    @Test
    void unsetTimeoutRestoresDefaultTimeout() {
        HttpConfig base = HttpConfig.builder().timeout(Duration.ofSeconds(7)).build();
        assertThat(base.getTimeout()).isEqualTo(Duration.ofSeconds(7));
        HttpConfig restored = base.toBuilder().unsetTimeout().build();
        assertThat(restored.getTimeout()).isEqualTo(Duration.ofSeconds(60));
    }

    @Test
    void unsetSslStrictRestoresDefault() {
        HttpConfig c = HttpConfig.builder().sslStrict(false).build();
        assertThat(c.isSslStrict()).isFalse();
        HttpConfig restored = c.toBuilder().unsetSslStrict().build();
        assertThat(restored.isSslStrict()).isTrue();
    }

    @Test
    void scopeSetterStoresScopeAndUrlPattern() {
        Pattern p = Pattern.compile(".*");
        HttpConfig c = HttpConfig.builder().scope("globalish", p).build();
        assertThat(c.getScope()).contains("globalish");
        assertThat(c.getUrlPattern()).contains(p);
    }

    @Test
    void mergedWithUnionsAndOverrides() {
        HttpConfig a = HttpConfig.builder()
                .header("X-A", "1")
                .query("q", "v1")
                .cookie("c1", "x")
                .basicAuth("alice", "p1")
                .apiKey("apk-A")
                .build();
        HttpConfig b = HttpConfig.builder()
                .header("X-B", "2")
                .query("q", "v2")
                .cookie("c1", "y")  // overwrite c1
                .basicAuth("bob", "p2")
                .apiKey("apk-B")
                .build();
        HttpConfig m = a.mergedWith(b);
        assertThat(m.getHeaders()).containsKey("X-A").containsKey("X-B");
        assertThat(m.getQuery().get("q")).containsExactly("v1", "v2");
        assertThat(m.getCookies()).containsEntry("c1", "y");
        assertThat(m.getAuth().get().user).isEqualTo("bob");
        assertThat(m.getApiKey()).contains("apk-B");
    }

    @Test
    void mergedWithProxyOverridesOnlyWhenSet() {
        HttpConfig.Proxy proxy = new HttpConfig.Proxy("p", 8080, "", "", "");
        HttpConfig a = HttpConfig.builder().proxy(proxy).build();
        HttpConfig b = HttpConfig.builder().header("X", "y").build();
        assertThat(a.mergedWith(b).getProxy()).contains(proxy);
        HttpConfig.Proxy proxy2 = new HttpConfig.Proxy("p2", 9090, "", "", "");
        HttpConfig c = HttpConfig.builder().proxy(proxy2).build();
        assertThat(a.mergedWith(c).getProxy().get().host).isEqualTo("p2");
    }

    @Test
    void toBuilderRoundtripPreservesEverything() {
        HttpConfig original = HttpConfig.builder()
                .header("H", "h")
                .query("q", "v")
                .cookie("c", "x")
                .timeout(Duration.ofSeconds(5))
                .sslStrict(false)
                .basicAuth("u", "p")
                .apiKey("k")
                .scope("s", Pattern.compile(".*"))
                .build();
        HttpConfig copy = original.toBuilder().build();
        assertThat(copy.getHeaders()).isEqualTo(original.getHeaders());
        assertThat(copy.getQuery()).isEqualTo(original.getQuery());
        assertThat(copy.getCookies()).isEqualTo(original.getCookies());
        assertThat(copy.getTimeout()).isEqualTo(original.getTimeout());
        assertThat(copy.isSslStrict()).isEqualTo(original.isSslStrict());
        assertThat(copy.getAuth().get().user).isEqualTo("u");
        assertThat(copy.getApiKey()).contains("k");
        assertThat(copy.getScope()).contains("s");
    }

    @Test
    void headersAndQueryReturnedMapsAreImmutable() {
        HttpConfig c = HttpConfig.builder().header("a", "1").query("b", "2").build();
        assertThatThrownBy(() -> c.getHeaders().put("x", Collections.singletonList("y")))
                .isInstanceOf(UnsupportedOperationException.class);
        assertThatThrownBy(() -> c.getQuery().put("x", Collections.singletonList("y")))
                .isInstanceOf(UnsupportedOperationException.class);
        assertThatThrownBy(() -> c.getCookies().put("x", "y"))
                .isInstanceOf(UnsupportedOperationException.class);
        // The list within is also immutable
        assertThatThrownBy(() -> c.getHeaders().get("a").add("more"))
                .isInstanceOf(UnsupportedOperationException.class);
    }

    @Test
    void toSafeStringRedactsSensitiveFields() {
        HttpConfig c = HttpConfig.builder()
                .basicAuth("alice", "very-secret")
                .header("Authorization", "Bearer xyz")
                .header("X-Api-Token", "sensitive")
                .header("X-Plain", "ok")
                .cookie("session", "v")
                .query("filter", "x")
                .apiKey("k")
                .proxy(new HttpConfig.Proxy("h", 1, "u", "pw", ""))
                .oauth2(HttpConfig.OAuth2.builder()
                        .clientId("cid")
                        .clientSecret("csec")
                        .audience("aud")
                        .build())
                .build();
        String s = c.toSafeString();
        assertThat(s).contains("alice");
        assertThat(s).doesNotContain("very-secret");
        assertThat(s).doesNotContain("Bearer xyz");
        assertThat(s).doesNotContain("sensitive");
        assertThat(s).contains("X-Plain=ok");
        assertThat(s).contains("session");
        assertThat(s).contains("filter");
        assertThat(s).contains("API key: ****");
        assertThat(s).contains("cid");
        assertThat(s).doesNotContain("csec");
        assertThat(s).contains("aud");
    }

    @Test
    void oauth2BuilderRejectsNonceLengthOutOfRange() {
        assertThatThrownBy(() -> HttpConfig.OAuth2.builder().nonceLength(7))
                .isInstanceOf(IllegalArgumentException.class);
        assertThatThrownBy(() -> HttpConfig.OAuth2.builder().nonceLength(65))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void oauth2BuilderAcceptsValidNonceLengthBoundaries() {
        HttpConfig.OAuth2 lo = HttpConfig.OAuth2.builder().nonceLength(8).build();
        HttpConfig.OAuth2 hi = HttpConfig.OAuth2.builder().nonceLength(64).build();
        assertThat(lo.nonceLength).isEqualTo(8);
        assertThat(hi.nonceLength).isEqualTo(64);
    }

    @Test
    void oauth2BuilderHandlesNullStrings() {
        HttpConfig.OAuth2 o = HttpConfig.OAuth2.builder()
                .clientId(null)
                .clientSecret(null)
                .clientSecretKeychain(null)
                .tokenUrl(null)
                .refreshUrl(null)
                .audience(null)
                .scopes(null)
                .build();
        assertThat(o.clientId).isEmpty();
        assertThat(o.clientSecret).isEmpty();
        assertThat(o.clientSecretKeychain).isEmpty();
        assertThat(o.tokenUrlOverride).isEmpty();
        assertThat(o.refreshUrlOverride).isEmpty();
        assertThat(o.audience).isEmpty();
        assertThat(o.scopesOverride).isEmpty();
    }

    @Test
    void oauth2PublicConstructorTreatsAllFieldsAsExplicit() {
        HttpConfig.OAuth2 base = HttpConfig.OAuth2.builder()
                .clientId("base")
                .nonceLength(32)
                .useForSpecFetch(false)
                .tokenEndpointAuthMethod(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC5849_OAUTH1_SIGNATURE)
                .build();
        HttpConfig.OAuth2 override = new HttpConfig.OAuth2(
                "override", "", "", "", "", "",
                Arrays.asList("a"), true,
                HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC6749_CLIENT_SECRET_BASIC,
                40);
        // Override is built via the public constructor → all flags explicit; merging onto base should win.
        HttpConfig merged = HttpConfig.builder().oauth2(base).build()
                .mergedWith(HttpConfig.builder().oauth2(override).build());
        HttpConfig.OAuth2 o = merged.getOAuth2().get();
        assertThat(o.clientId).isEqualTo("override");
        assertThat(o.nonceLength).isEqualTo(40);
        assertThat(o.useForSpecFetch).isTrue();
        assertThat(o.tokenEndpointAuthMethod)
                .isEqualTo(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC6749_CLIENT_SECRET_BASIC);
    }

    @Test
    void oauth2MergedOntoNullBaseReturnsThis() {
        HttpConfig.OAuth2 only = HttpConfig.OAuth2.builder().clientId("solo").build();
        HttpConfig merged = HttpConfig.builder().build()
                .mergedWith(HttpConfig.builder().oauth2(only).build());
        assertThat(merged.getOAuth2().get().clientId).isEqualTo("solo");
    }

    @Test
    void httpConfigBuilderAuthSetterAcceptsNull() {
        HttpConfig c = HttpConfig.builder().basicAuth("u", "p").auth(null).build();
        assertThat(c.getAuth()).isEmpty();
    }
}
