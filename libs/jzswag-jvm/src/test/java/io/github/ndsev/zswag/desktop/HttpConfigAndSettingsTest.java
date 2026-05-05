package io.github.ndsev.zswag.desktop;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpSettings;
import org.junit.jupiter.api.Test;

import java.time.Duration;
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

    @Test
    void mergedWithPreservesBaseSslStrictFalseWhenOtherUntouched() {
        // Regression: previously `mergedWith` overrode sslStrict only when other.sslStrict==false,
        // which couldn't distinguish "explicitly true" from "default". A wildcard scope that disables
        // strict SSL in dev should not be poisoned by a later merge that didn't touch sslStrict.
        HttpConfig base = HttpConfig.builder().sslStrict(false).build();
        HttpConfig other = HttpConfig.builder().header("X", "y").build();
        assertThat(base.mergedWith(other).isSslStrict()).isFalse();
    }

    @Test
    void mergedWithLetsOtherReEnableSslStrict() {
        // Regression: previously the merge could only ever turn sslStrict OFF (the !other.sslStrict
        // branch was one-way), so a config explicitly setting sslStrict(true) couldn't restore strictness.
        HttpConfig base = HttpConfig.builder().sslStrict(false).build();
        HttpConfig other = HttpConfig.builder().sslStrict(true).build();
        assertThat(base.mergedWith(other).isSslStrict()).isTrue();
    }

    @Test
    void mergedWithPreservesBaseTimeoutWhenOtherUntouched() {
        // Regression: previously `mergedWith` compared other.timeout to defaultTimeout() and
        // overrode only on inequality, which (a) loses an explicit "set to default" and (b) loses
        // a non-default base when the merging-in side never touched timeout.
        HttpConfig base = HttpConfig.builder().timeout(Duration.ofSeconds(5)).build();
        HttpConfig other = HttpConfig.builder().header("X", "y").build();
        assertThat(base.mergedWith(other).getTimeout()).isEqualTo(Duration.ofSeconds(5));
    }

    @Test
    void mergedWithLetsOtherOverrideTimeout() {
        HttpConfig base = HttpConfig.builder().timeout(Duration.ofSeconds(5)).build();
        HttpConfig other = HttpConfig.builder().timeout(Duration.ofSeconds(20)).build();
        assertThat(base.mergedWith(other).getTimeout()).isEqualTo(Duration.ofSeconds(20));
    }

    @Test
    void oauth2MergedOntoPreservesBaseTokenEndpointAuthMethodWhenThisDidNotSetIt() {
        // Regression: previously `OAuth2.mergedOnto` always took useForSpecFetch /
        // tokenEndpointAuthMethod / nonceLength from `this`, so any merge with an OAuth2 built
        // without those setters would silently overwrite a non-default base value.
        HttpConfig.OAuth2 base = HttpConfig.OAuth2.builder()
                .clientId("base")
                .tokenEndpointAuthMethod(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC5849_OAUTH1_SIGNATURE)
                .nonceLength(32)
                .useForSpecFetch(false)
                .build();
        HttpConfig.OAuth2 override = HttpConfig.OAuth2.builder().clientId("override").build();
        HttpConfig merged = HttpConfig.builder().oauth2(base).build()
                .mergedWith(HttpConfig.builder().oauth2(override).build());
        HttpConfig.OAuth2 oauth = merged.getOAuth2().get();
        assertThat(oauth.tokenEndpointAuthMethod)
                .isEqualTo(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC5849_OAUTH1_SIGNATURE);
        assertThat(oauth.nonceLength).isEqualTo(32);
        assertThat(oauth.useForSpecFetch).isFalse();
        assertThat(oauth.clientId).isEqualTo("override");
    }

    @Test
    void oauth2MergedOntoLetsThisOverrideExplicitlySetFields() {
        HttpConfig.OAuth2 base = HttpConfig.OAuth2.builder()
                .clientId("base")
                .nonceLength(32)
                .build();
        HttpConfig.OAuth2 override = HttpConfig.OAuth2.builder()
                .clientId("override")
                .nonceLength(48)
                .build();
        HttpConfig merged = HttpConfig.builder().oauth2(base).build()
                .mergedWith(HttpConfig.builder().oauth2(override).build());
        assertThat(merged.getOAuth2().get().nonceLength).isEqualTo(48);
    }
}
