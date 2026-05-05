package com.ndsev.zswag.api;

import org.junit.jupiter.api.Test;

import java.util.Arrays;
import java.util.regex.Pattern;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class HttpSettingsTest {

    @Test
    void emptyHasNoEntries() {
        HttpSettings s = HttpSettings.empty();
        assertThat(s.getEntries()).isEmpty();
        assertThat(s.forUrl("https://anywhere/")).isNotNull();
    }

    @Test
    void entriesAreImmutable() {
        HttpSettings s = HttpSettings.empty();
        assertThatThrownBy(() -> s.getEntries().add(HttpConfig.empty()))
                .isInstanceOf(UnsupportedOperationException.class);
    }

    @Test
    void compileScopeWildcardMatchesEverything() {
        Pattern p = HttpSettings.compileScope("*");
        assertThat(p.matcher("anything").matches()).isTrue();
        assertThat(p.matcher("").matches()).isTrue();
    }

    @Test
    void compileScopeEscapesDotsAndMetachars() {
        // Dots are literal, parens/brackets/braces/?/+/-/!/^/$/| are escaped
        Pattern p = HttpSettings.compileScope("a.b+c?[]{}|()-!^$");
        assertThat(p.matcher("a.b+c?[]{}|()-!^$").matches()).isTrue();
        assertThat(p.matcher("aXb+c?[]{}|()-!^$").matches()).isFalse();
    }

    @Test
    void compileScopeEscapesBackslash() {
        Pattern p = HttpSettings.compileScope("a\\b");
        assertThat(p.matcher("a\\b").matches()).isTrue();
    }

    @Test
    void compileScopeMatchesGlobs() {
        Pattern p = HttpSettings.compileScope("https://*.foo.com/*");
        assertThat(p.matcher("https://api.foo.com/data").matches()).isTrue();
        assertThat(p.matcher("https://foo.com/").matches()).isFalse();
        assertThat(p.matcher("http://api.foo.com/").matches()).isFalse();
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
    void forUrlAppliesEntryWithoutPattern() {
        HttpConfig anyEntry = HttpConfig.builder().header("X", "y").build();
        HttpSettings s = new HttpSettings(Arrays.asList(anyEntry));
        assertThat(s.forUrl("https://anywhere/").getHeader("X")).contains("y");
    }
}
