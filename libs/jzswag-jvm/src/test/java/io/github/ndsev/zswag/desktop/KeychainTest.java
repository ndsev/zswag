package io.github.ndsev.zswag.desktop;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import static org.assertj.core.api.Assertions.assertThatThrownBy;

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
        assertThatThrownBy(() -> Keychain.load("", "user"))
                .isInstanceOf(Keychain.KeychainException.class)
                .hasMessageContaining("service identifier");
    }

    @Test
    void unknownPlatformThrowsUnsupported() {
        System.setProperty("os.name", "PalmOS");
        assertThatThrownBy(() -> Keychain.load("svc", "user"))
                .isInstanceOf(Keychain.KeychainException.class)
                .hasMessageContaining("unsupported platform");
    }

    @Test
    void windowsThrowsNotImplemented() {
        System.setProperty("os.name", "Windows 10");
        assertThatThrownBy(() -> Keychain.load("svc", "user"))
                .isInstanceOf(Keychain.KeychainException.class)
                .hasMessageContaining("Windows credential manager");
    }

    @Test
    void linuxThrowsWhenSecretToolMissing() {
        // On the CI runner secret-tool is not installed, so this exercises the
        // "ProcessBuilder.start IOException → 'not installed or not on PATH'" branch.
        // If a developer happens to have secret-tool installed locally, the test asserts a
        // generic KeychainException — either way, we exercise loadLinux().
        System.setProperty("os.name", "Linux");
        assertThatThrownBy(() -> Keychain.load("zswag.test.does-not-exist", "no.such.user"))
                .isInstanceOf(Keychain.KeychainException.class);
    }

    @Test
    void macOsThrowsWhenSecurityToolMissingOrEntryAbsent() {
        // 'security' is macOS-only and unlikely on Linux CI; this exercises the IOException path
        // ("not installed or not on PATH") on non-mac runners.
        System.setProperty("os.name", "Mac OS X");
        assertThatThrownBy(() -> Keychain.load("zswag.test.does-not-exist", "no.such.user"))
                .isInstanceOf(Keychain.KeychainException.class);
    }

    @Test
    void keychainExceptionMessageAndCausePreserved() {
        Keychain.KeychainException simple = new Keychain.KeychainException("just msg");
        assertThatThrownBy(() -> { throw simple; })
                .isInstanceOf(Keychain.KeychainException.class)
                .hasMessage("just msg");
        Throwable cause = new RuntimeException("inner");
        Keychain.KeychainException withCause = new Keychain.KeychainException("outer", cause);
        assertThatThrownBy(() -> { throw withCause; })
                .isInstanceOf(Keychain.KeychainException.class)
                .hasCause(cause);
    }
}
