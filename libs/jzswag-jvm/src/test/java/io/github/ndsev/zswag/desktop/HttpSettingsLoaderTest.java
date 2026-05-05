package io.github.ndsev.zswag.desktop;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpSettings;
import org.junit.jupiter.api.Test;

import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Verifies that the YAML schema accepted by HttpSettingsLoader matches the C++/Python
 * reference exactly so that the same {@code HTTP_SETTINGS_FILE} can drive all clients.
 */
class HttpSettingsLoaderTest {

    @Test
    void emptyRootProducesEmptySettings() {
        HttpSettings s = HttpSettingsLoader.parseRoot(null);
        assertThat(s.getEntries()).isEmpty();
    }

    @Test
    void mapRootWithoutHttpSettingsKeyProducesEmpty() {
        Map<String, Object> root = new LinkedHashMap<>();
        root.put("unrelated", 42);
        HttpSettings s = HttpSettingsLoader.parseRoot(root);
        assertThat(s.getEntries()).isEmpty();
    }

    @Test
    void mapRootWithHttpSettingsKeyParsesAllEntries() {
        Map<String, Object> entry1 = entry("https://*.foo.com/*",
                "basic-auth", entry(null, "user", "alice", "password", "secret"));
        Map<String, Object> entry2 = entry("https://api.bar.com/*",
                "headers", entry(null, "X-Trace", "abc"));
        Map<String, Object> root = new LinkedHashMap<>();
        root.put("http-settings", Arrays.asList(entry1, entry2));

        HttpSettings s = HttpSettingsLoader.parseRoot(root);
        assertThat(s.getEntries()).hasSize(2);
        assertThat(s.getEntries().get(0).getAuth()).isPresent();
        assertThat(s.getEntries().get(0).getAuth().get().user).isEqualTo("alice");
        assertThat(s.getEntries().get(0).getAuth().get().password).isEqualTo("secret");
        assertThat(s.getEntries().get(1).getHeaders()).containsKey("X-Trace");
    }

    @Test
    void legacyListRootIsAccepted() {
        // Matches C++ http-settings.cpp:466-469 backwards-compat path.
        Map<String, Object> entry = entry("*",
                "headers", entry(null, "X-Old", "v"));
        HttpSettings s = HttpSettingsLoader.parseRoot(Arrays.asList(entry));
        assertThat(s.getEntries()).hasSize(1);
        assertThat(s.getEntries().get(0).getHeaders()).containsKey("X-Old");
    }

    @Test
    void scopeDefaultsToWildcardWhenAbsent() {
        Map<String, Object> root = singleEntry(null);
        HttpSettings s = HttpSettingsLoader.parseRoot(root);
        assertThat(s.getEntries().get(0).getScope()).contains("*");
    }

    @Test
    void rawUrlRegexIsPreserved() {
        Map<String, Object> entry = entry(null,
                "url", "^https://api\\.example\\.com/.*$",
                "headers", entry(null, "X-Y", "z"));
        HttpSettings s = HttpSettingsLoader.parseRoot(asHttpSettings(entry));
        // url-form entries have no scope (only urlPattern); compileScope wasn't applied.
        assertThat(s.getEntries().get(0).getScope()).isNotPresent();
        assertThat(s.getEntries().get(0).getUrlPattern()).isPresent();
    }

    @Test
    void basicAuthRequiresUser() {
        Map<String, Object> entry = entry("*",
                "basic-auth", entry(null, "password", "secret"));
        assertThatThrownBy(() -> HttpSettingsLoader.parseRoot(asHttpSettings(entry)))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("basic-auth requires 'user'");
    }

    @Test
    void basicAuthRequiresPasswordOrKeychain() {
        Map<String, Object> entry = entry("*",
                "basic-auth", entry(null, "user", "alice"));
        assertThatThrownBy(() -> HttpSettingsLoader.parseRoot(asHttpSettings(entry)))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("password");
    }

    @Test
    void basicAuthAcceptsKeychain() {
        Map<String, Object> entry = entry("*",
                "basic-auth", entry(null, "user", "alice", "keychain", "my-service"));
        HttpSettings s = HttpSettingsLoader.parseRoot(asHttpSettings(entry));
        HttpConfig.BasicAuthentication a = s.getEntries().get(0).getAuth().get();
        assertThat(a.password).isEmpty();
        assertThat(a.keychain).isEqualTo("my-service");
    }

    @Test
    void proxyParsedWithCredentials() {
        Map<String, Object> entry = entry("*",
                "proxy", entry(null, "host", "proxy.local", "port", 3128,
                        "user", "u", "password", "p"));
        HttpSettings s = HttpSettingsLoader.parseRoot(asHttpSettings(entry));
        HttpConfig.Proxy p = s.getEntries().get(0).getProxy().get();
        assertThat(p.host).isEqualTo("proxy.local");
        assertThat(p.port).isEqualTo(3128);
        assertThat(p.user).isEqualTo("u");
        assertThat(p.password).isEqualTo("p");
    }

    @Test
    void proxyRequiresHostAndPort() {
        Map<String, Object> entry = entry("*",
                "proxy", entry(null, "port", 3128));
        assertThatThrownBy(() -> HttpSettingsLoader.parseRoot(asHttpSettings(entry)))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void apiKeyParsedAsString() {
        Map<String, Object> entry = entry("*", "api-key", "my-token");
        HttpSettings s = HttpSettingsLoader.parseRoot(asHttpSettings(entry));
        assertThat(s.getEntries().get(0).getApiKey()).contains("my-token");
    }

    @Test
    void oauth2ParsedFully() {
        Map<String, Object> oauth2 = new LinkedHashMap<>();
        oauth2.put("clientId", "my-id");
        oauth2.put("clientSecret", "secret");
        oauth2.put("tokenUrl", "https://issuer/oauth/token");
        oauth2.put("audience", "https://api/");
        oauth2.put("scope", Arrays.asList("read", "write"));
        oauth2.put("useForSpecFetch", false);
        Map<String, Object> tea = new LinkedHashMap<>();
        tea.put("method", "rfc5849-oauth1-signature");
        tea.put("nonceLength", 32);
        oauth2.put("tokenEndpointAuth", tea);

        Map<String, Object> entry = entry("https://*.example.com/*", "oauth2", oauth2);
        HttpSettings s = HttpSettingsLoader.parseRoot(asHttpSettings(entry));

        HttpConfig.OAuth2 cfg = s.getEntries().get(0).getOAuth2().get();
        assertThat(cfg.clientId).isEqualTo("my-id");
        assertThat(cfg.clientSecret).isEqualTo("secret");
        assertThat(cfg.tokenUrlOverride).isEqualTo("https://issuer/oauth/token");
        assertThat(cfg.audience).isEqualTo("https://api/");
        assertThat(cfg.scopesOverride).containsExactly("read", "write");
        assertThat(cfg.useForSpecFetch).isFalse();
        assertThat(cfg.tokenEndpointAuthMethod)
                .isEqualTo(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC5849_OAUTH1_SIGNATURE);
        assertThat(cfg.nonceLength).isEqualTo(32);
    }

    @Test
    void oauth2RejectsUnknownAuthMethod() {
        Map<String, Object> tea = new LinkedHashMap<>();
        tea.put("method", "unknown-scheme");
        Map<String, Object> oauth2 = new LinkedHashMap<>();
        oauth2.put("clientId", "x");
        oauth2.put("tokenEndpointAuth", tea);
        Map<String, Object> entry = entry("*", "oauth2", oauth2);
        assertThatThrownBy(() -> HttpSettingsLoader.parseRoot(asHttpSettings(entry)))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("Unknown tokenEndpointAuth method");
    }

    @Test
    void oauth2NonceLengthOutOfRange() {
        Map<String, Object> tea = new LinkedHashMap<>();
        tea.put("method", "rfc5849-oauth1-signature");
        tea.put("nonceLength", 4);  // below minimum
        Map<String, Object> oauth2 = new LinkedHashMap<>();
        oauth2.put("clientId", "x");
        oauth2.put("tokenEndpointAuth", tea);
        Map<String, Object> entry = entry("*", "oauth2", oauth2);
        assertThatThrownBy(() -> HttpSettingsLoader.parseRoot(asHttpSettings(entry)))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("nonceLength must be between 8 and 64");
    }

    @Test
    void oauth2DefaultsToBasicAuthAndUseForSpecFetchTrue() {
        Map<String, Object> oauth2 = new LinkedHashMap<>();
        oauth2.put("clientId", "x");
        oauth2.put("clientSecret", "y");
        Map<String, Object> entry = entry("*", "oauth2", oauth2);
        HttpSettings s = HttpSettingsLoader.parseRoot(asHttpSettings(entry));
        HttpConfig.OAuth2 cfg = s.getEntries().get(0).getOAuth2().get();
        assertThat(cfg.tokenEndpointAuthMethod)
                .isEqualTo(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC6749_CLIENT_SECRET_BASIC);
        assertThat(cfg.useForSpecFetch).isTrue();
        assertThat(cfg.nonceLength).isEqualTo(16);
    }

    // --- helpers --------------------------------------------------------

    /** Builds a single-entry http-settings root map, with the given inner entry. */
    private static Map<String, Object> singleEntry(String scope) {
        Map<String, Object> e = new LinkedHashMap<>();
        if (scope != null) e.put("scope", scope);
        return asHttpSettings(e);
    }

    private static Map<String, Object> asHttpSettings(Map<String, Object> entry) {
        Map<String, Object> root = new LinkedHashMap<>();
        root.put("http-settings", java.util.Collections.singletonList(entry));
        return root;
    }

    /** Builds a single-entry http-settings root map with the given key/value pairs. */
    private static Map<String, Object> entry(String scope, Object... kvs) {
        Map<String, Object> map = new LinkedHashMap<>();
        if (scope != null) map.put("scope", scope);
        for (int i = 0; i < kvs.length; i += 2) {
            map.put((String) kvs[i], kvs[i + 1]);
        }
        return map;
    }
}
