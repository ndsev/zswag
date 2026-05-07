package io.github.ndsev.zswag.jvm;

import io.github.ndsev.zswag.api.KeychainException;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.parallel.Isolated;

import static org.assertj.core.api.Assertions.assertThatThrownBy;

// @Isolated guards against parallel test execution: this class mutates the
// global os.name system property which other tests might read concurrently.
@Isolated
class KeychainTest {

    private String savedOsName;

    @BeforeEach
    void saveOsName() {
        savedOsName = System.getProperty("os.name");
    }

    @AfterEach
    void restoreOsName() {
        if (savedOsName != null) System.setProperty("os.name", savedOsName);
    }

    @Test
    void emptyServiceThrows() {
        assertThatThrownBy(() -> new Keychain().load("", "user"))
                .isInstanceOf(KeychainException.class)
                .hasMessageContaining("service identifier");
    }

    @Test
    void unknownPlatformThrowsUnsupported() {
        System.setProperty("os.name", "PalmOS");
        assertThatThrownBy(() -> new Keychain().load("svc", "user"))
                .isInstanceOf(KeychainException.class)
                .hasMessageContaining("unsupported platform");
    }

    @Test
    void windowsThrowsNotImplemented() {
        System.setProperty("os.name", "Windows 10");
        assertThatThrownBy(() -> new Keychain().load("svc", "user"))
                .isInstanceOf(KeychainException.class)
                .hasMessageContaining("Windows credential manager");
    }

    @Test
    void linuxThrowsWhenSecretToolMissing() {
        // On the CI runner secret-tool is not installed, so this exercises the
        // "ProcessBuilder.start IOException → 'not installed or not on PATH'" branch.
        // If a developer happens to have secret-tool installed locally, the test asserts a
        // generic KeychainException — either way, we exercise loadLinux().
        System.setProperty("os.name", "Linux");
        assertThatThrownBy(() -> new Keychain().load("zswag.test.does-not-exist", "no.such.user"))
                .isInstanceOf(KeychainException.class);
    }

    @Test
    void macOsThrowsWhenSecurityToolMissingOrEntryAbsent() {
        // 'security' is macOS-only and unlikely on Linux CI; this exercises the IOException path
        // ("not installed or not on PATH") on non-mac runners.
        System.setProperty("os.name", "Mac OS X");
        assertThatThrownBy(() -> new Keychain().load("zswag.test.does-not-exist", "no.such.user"))
                .isInstanceOf(KeychainException.class);
    }

    @Test
    void keychainExceptionMessageAndCausePreserved() {
        KeychainException simple = new KeychainException("just msg");
        assertThatThrownBy(() -> { throw simple; })
                .isInstanceOf(KeychainException.class)
                .hasMessage("just msg");
        Throwable cause = new RuntimeException("inner");
        KeychainException withCause = new KeychainException("outer", cause);
        assertThatThrownBy(() -> { throw withCause; })
                .isInstanceOf(KeychainException.class)
                .hasCause(cause);
    }
}
