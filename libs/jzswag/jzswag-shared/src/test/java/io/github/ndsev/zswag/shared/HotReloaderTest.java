package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.HttpSettings;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.FileTime;

import static org.assertj.core.api.Assertions.assertThat;

/**
 * Verifies that {@link HttpSettingsLoader.HotReloader} re-reads the source file when
 * its modification time advances — matches C++ {@code Settings::operator[]} behaviour
 * for credential-rotation use cases.
 */
class HotReloaderTest {

    @TempDir
    Path tmp;

    private static final String SETTINGS_V1 = String.join("\n",
            "http-settings:",
            "  - scope: '*'",
            "    headers:",
            "      X-Version: v1"
    );

    private static final String SETTINGS_V2 = String.join("\n",
            "http-settings:",
            "  - scope: '*'",
            "    headers:",
            "      X-Version: v2"
    );

    @Test
    void initialLoadReturnsSettingsFromFile() throws Exception {
        Path file = tmp.resolve("settings.yaml");
        Files.writeString(file, SETTINGS_V1);
        HttpSettings initial = HttpSettingsLoader.loadFromFile(file);
        HttpSettingsLoader.HotReloader r = HttpSettingsLoader.HotReloader.of(file, initial);
        assertThat(r.current().forUrl("https://anything")
                .getHeader("X-Version")).contains("v1");
    }

    @Test
    void unchangedFileReturnsSameInstance() throws Exception {
        Path file = tmp.resolve("settings.yaml");
        Files.writeString(file, SETTINGS_V1);
        HttpSettings initial = HttpSettingsLoader.loadFromFile(file);
        HttpSettingsLoader.HotReloader r = HttpSettingsLoader.HotReloader.of(file, initial);
        HttpSettings first = r.current();
        HttpSettings second = r.current();
        // No reload happened — same instance (identity equality).
        assertThat(second).isSameAs(first);
    }

    @Test
    void advancedMtimeTriggersReload() throws Exception {
        Path file = tmp.resolve("settings.yaml");
        Files.writeString(file, SETTINGS_V1);
        HttpSettings initial = HttpSettingsLoader.loadFromFile(file);
        HttpSettingsLoader.HotReloader r = HttpSettingsLoader.HotReloader.of(file, initial);
        assertThat(r.current().forUrl("https://anything")
                .getHeader("X-Version")).contains("v1");

        // Overwrite and bump mtime explicitly (some filesystems coalesce same-second writes).
        Files.writeString(file, SETTINGS_V2);
        Files.setLastModifiedTime(file, FileTime.fromMillis(System.currentTimeMillis() + 5000));

        assertThat(r.current().forUrl("https://anything")
                .getHeader("X-Version")).contains("v2");
    }

    @Test
    void reloadFailureKeepsPreviousSnapshot() throws Exception {
        Path file = tmp.resolve("settings.yaml");
        Files.writeString(file, SETTINGS_V1);
        HttpSettings initial = HttpSettingsLoader.loadFromFile(file);
        HttpSettingsLoader.HotReloader r = HttpSettingsLoader.HotReloader.of(file, initial);

        // Corrupt the file so re-parsing fails; bump mtime to trigger the reload path.
        Files.writeString(file, "this: is\nnot: { valid yaml: deliberately broken: oh: no: :::");
        Files.setLastModifiedTime(file, FileTime.fromMillis(System.currentTimeMillis() + 5000));

        // Should keep the previous v1 snapshot, not drop to empty.
        assertThat(r.current().forUrl("https://anything")
                .getHeader("X-Version")).contains("v1");
    }

    @Test
    void noSourcePathSkipsReloadChecks() throws Exception {
        HttpSettings snapshot = HttpSettings.empty();
        HttpSettingsLoader.HotReloader r = HttpSettingsLoader.HotReloader.of(null, snapshot);
        assertThat(r.current()).isSameAs(snapshot);
        // Repeated calls — same instance, never reloads.
        assertThat(r.current()).isSameAs(snapshot);
    }
}
