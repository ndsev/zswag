package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.HttpConfig;
import com.ndsev.zswag.api.HttpSettings;
import org.junit.jupiter.api.Test;

import java.util.Arrays;
import java.util.regex.Pattern;

import static org.assertj.core.api.Assertions.assertThat;

class HttpConfigAndSettingsTest {

    @Test
    void mergedWithUnionsHeadersAndQuery() {
        HttpConfig a = HttpConfig.builder()
                .header("X-A", "1")
                .query("q", "v1")
                .build();
        HttpConfig b = HttpConfig.builder()
                .header("X-B", "2")
                .query("q", "v2")
                .build();
        HttpConfig merged = a.mergedWith(b);
        assertThat(merged.getHeaders()).containsKey("X-A").containsKey("X-B");
        // Multi-valued union: q has both v1 and v2.
        assertThat(merged.getQuery().get("q")).containsExactly("v1", "v2");
    }

    @Test
    void mergedWithOverwritesAuthAndProxy() {
        HttpConfig a = HttpConfig.builder().basicAuth("alice", "p1").build();
        HttpConfig b = HttpConfig.builder().basicAuth("bob", "p2").build();
        HttpConfig merged = a.mergedWith(b);
        assertThat(merged.getAuth().get().user).isEqualTo("bob");
        assertThat(merged.getAuth().get().password).isEqualTo("p2");
    }

    @Test
    void mergedWithKeepsBaseAuthIfOtherHasNone() {
        HttpConfig a = HttpConfig.builder().basicAuth("alice", "p1").build();
        HttpConfig b = HttpConfig.builder().header("X-Y", "z").build();
        HttpConfig merged = a.mergedWith(b);
        assertThat(merged.getAuth().get().user).isEqualTo("alice");
    }

    @Test
    void oauth2SubFieldsMergedFieldByField() {
        HttpConfig a = HttpConfig.builder()
                .oauth2(HttpConfig.OAuth2.builder().clientId("base").audience("aud-1").build())
                .build();
        HttpConfig b = HttpConfig.builder()
                .oauth2(HttpConfig.OAuth2.builder().clientId("override").build())
                .build();
        HttpConfig merged = a.mergedWith(b);
        HttpConfig.OAuth2 oauth = merged.getOAuth2().get();
        assertThat(oauth.clientId).isEqualTo("override");
        assertThat(oauth.audience).isEqualTo("aud-1"); // preserved from base since b had none
    }

    @Test
    void compileScopeMatchesGlobs() {
        Pattern p = HttpSettings.compileScope("https://*.foo.com/*");
        assertThat(p.matcher("https://api.foo.com/data").matches()).isTrue();
        // The literal dot before foo is required: "foo.com" alone does NOT match "*.foo.com".
        assertThat(p.matcher("https://foo.com/").matches()).isFalse();
        assertThat(p.matcher("http://api.foo.com/").matches()).isFalse();   // protocol mismatch
        assertThat(p.matcher("https://bar.example.com/").matches()).isFalse();
    }

    @Test
    void compileScopeEscapesRegexMetachars() {
        Pattern p = HttpSettings.compileScope("a.b+c");
        assertThat(p.matcher("a.b+c").matches()).isTrue();
        assertThat(p.matcher("aXbXc").matches()).isFalse();
    }

    @Test
    void compileScopeWildcardMatchesAll() {
        Pattern p = HttpSettings.compileScope("*");
        assertThat(p.matcher("anything").matches()).isTrue();
        assertThat(p.matcher("").matches()).isTrue();
    }

    @Test
    void forUrlMergesAllMatchingScopes() {
        HttpConfig wildcard = HttpConfig.builder()
                .scope("*", HttpSettings.compileScope("*"))
                .header("X-Generic", "global")
                .build();
        HttpConfig fooSpecific = HttpConfig.builder()
                .scope("https://*.foo.com/*", HttpSettings.compileScope("https://*.foo.com/*"))
                .header("X-Foo", "yes")
                .build();
        HttpSettings s = new HttpSettings(Arrays.asList(wildcard, fooSpecific));

        HttpConfig forFoo = s.forUrl("https://api.foo.com/x");
        assertThat(forFoo.getHeaders()).containsKey("X-Generic").containsKey("X-Foo");

        HttpConfig forOther = s.forUrl("https://bar.com/y");
        assertThat(forOther.getHeaders()).containsKey("X-Generic").doesNotContainKey("X-Foo");
    }

    @Test
    void emptySettingsForUrlReturnsEmptyConfig() {
        HttpConfig c = HttpSettings.empty().forUrl("https://anywhere/");
        assertThat(c.getHeaders()).isEmpty();
        assertThat(c.getAuth()).isNotPresent();
    }
}
