package com.ndsev.zswag.desktop;

import org.jetbrains.annotations.NotNull;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * OS keychain integration: load/store/remove credentials. Mirrors C++
 * {@code httpcl::secret} (which wraps the {@code keychain} library).
 *
 * <p>Implementation strategy: shells out to the platform-native keychain CLI
 * (no JNI). Linux: {@code secret-tool}; macOS: {@code security}; Windows:
 * {@code cmdkey}/{@code powershell}.
 *
 * <p>If the platform tool is unavailable or returns no entry, callers see a
 * {@link KeychainException} with a clear message — preferable to silently
 * sending an empty password.
 */
public final class Keychain {
    private static final Logger logger = LoggerFactory.getLogger(Keychain.class);

    /** Matches C++ {@code KEYCHAIN_PACKAGE} so secrets stored by C++ are visible to Java. */
    static final String PACKAGE = "lib.openapi.zserio.client";

    private static final long TIMEOUT_SECONDS = 60;

    private Keychain() {}

    /**
     * Loads a password for {@code (service, user)} from the platform keychain.
     * Throws if the keychain tool is missing or the entry doesn't exist.
     */
    @NotNull
    public static String load(@NotNull String service, @NotNull String user) {
        if (service.isEmpty()) {
            throw new KeychainException("keychain: service identifier must not be empty");
        }
        logger.debug("Loading secret (service={}, user={}) ...", service, user);
        Os os = detectOs();
        try {
            switch (os) {
                case LINUX:
                    return loadLinux(service, user);
                case MACOS:
                    return loadMacOs(service, user);
                case WINDOWS:
                    return loadWindows(service, user);
                default:
                    throw new KeychainException("keychain: unsupported platform " + System.getProperty("os.name"));
            }
        } catch (IOException | InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new KeychainException("keychain: failed to load secret: " + e.getMessage(), e);
        }
    }

    private static String loadLinux(String service, String user) throws IOException, InterruptedException {
        // secret-tool lookup package <PACKAGE> service <service> user <user>
        ProcessBuilder pb = new ProcessBuilder("secret-tool", "lookup",
                "package", PACKAGE,
                "service", service,
                "user", user);
        return runReadStdout(pb, "secret-tool");
    }

    private static String loadMacOs(String service, String user) throws IOException, InterruptedException {
        // security find-generic-password -s <service> -a <user> -w
        ProcessBuilder pb = new ProcessBuilder("security", "find-generic-password",
                "-s", service,
                "-a", user,
                "-w");
        return runReadStdout(pb, "security").trim();
    }

    private static String loadWindows(String service, String user) {
        // Windows credential manager lookup is awkward without PowerShell module access.
        throw new KeychainException("keychain: Windows credential manager lookup is not yet implemented; use cleartext password");
    }

    private static String runReadStdout(@NotNull ProcessBuilder pb, @NotNull String tool) throws IOException, InterruptedException {
        pb.redirectErrorStream(false);
        Process p;
        try {
            p = pb.start();
        } catch (IOException e) {
            throw new KeychainException("keychain: '" + tool + "' is not installed or not on PATH", e);
        }
        if (!p.waitFor(TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
            p.destroyForcibly();
            throw new KeychainException("keychain: '" + tool + "' timed out after " + TIMEOUT_SECONDS + "s");
        }
        StringBuilder out = new StringBuilder();
        try (BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream(), StandardCharsets.UTF_8))) {
            String line;
            while ((line = r.readLine()) != null) out.append(line).append('\n');
        }
        if (p.exitValue() != 0) {
            String stderr;
            try (BufferedReader r = new BufferedReader(new InputStreamReader(p.getErrorStream(), StandardCharsets.UTF_8))) {
                StringBuilder e = new StringBuilder();
                String line;
                while ((line = r.readLine()) != null) e.append(line).append('\n');
                stderr = e.toString().trim();
            }
            throw new KeychainException("keychain: '" + tool + "' exited " + p.exitValue() +
                    (stderr.isEmpty() ? "" : ": " + stderr));
        }
        // Strip a trailing newline from the password (secret-tool always appends one).
        String s = out.toString();
        if (s.endsWith("\n")) s = s.substring(0, s.length() - 1);
        return s;
    }

    private enum Os { LINUX, MACOS, WINDOWS, UNKNOWN }

    private static Os detectOs() {
        String name = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        if (name.contains("linux")) return Os.LINUX;
        if (name.contains("mac")) return Os.MACOS;
        if (name.contains("win")) return Os.WINDOWS;
        return Os.UNKNOWN;
    }

    /** Thrown when a keychain lookup fails. */
    public static class KeychainException extends RuntimeException {
        public KeychainException(String message) { super(message); }
        public KeychainException(String message, Throwable cause) { super(message, cause); }
    }
}
