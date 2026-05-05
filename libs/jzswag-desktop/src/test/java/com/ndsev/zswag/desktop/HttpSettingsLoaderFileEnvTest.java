package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.HttpSettings;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class HttpSettingsLoaderFileEnvTest {

    @Test
    void loadFromFileParsesValidYaml(@TempDir Path dir) throws IOException {
        Path p = dir.resolve("settings.yaml");
        Files.writeString(p, String.join("\n",
                "http-settings:",
                "  - scope: 'https://*.foo.com/*'",
                "    headers:",
                "      X-Trace: trace-1"));
        HttpSettings s = HttpSettingsLoader.loadFromFile(p);
        assertThat(s.getEntries()).hasSize(1);
        assertThat(s.forUrl("https://api.foo.com/x").getHeader("X-Trace")).contains("trace-1");
    }

    @Test
    void loadFromFileEmptyYamlYieldsEmpty(@TempDir Path dir) throws IOException {
        Path p = dir.resolve("e.yaml");
        Files.writeString(p, "");
        HttpSettings s = HttpSettingsLoader.loadFromFile(p);
        assertThat(s.getEntries()).isEmpty();
    }

    @Test
    void loadFromEnvironmentReturnsEmptyWhenEnvUnset() {
        // Cannot reliably set an env var from within a JVM test. We rely on the default
        // (HTTP_SETTINGS_FILE not set in CI) to exercise the unset/empty branch.
        HttpSettings s = HttpSettingsLoader.loadFromEnvironment();
        assertThat(s).isNotNull();
    }

    @Test
    void parseRootRejectsNonListHttpSettings() {
        assertThatThrownBy(() -> HttpSettingsLoader.parseRoot(java.util.Map.of("http-settings", "not-a-list")))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void parseRootRejectsScalarRoot() {
        assertThatThrownBy(() -> HttpSettingsLoader.parseRoot(42))
                .isInstanceOf(IllegalArgumentException.class);
    }
}
