package io.github.ndsev.zswag.jvm;

import io.github.ndsev.zswag.api.IKeychain;
import io.github.ndsev.zswag.api.KeychainException;
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
 * JVM keychain integration: load credentials from the OS-native credential
 * store. Mirrors C++ {@code httpcl::secret} (which wraps the {@code keychain}
 * library).
 *
 * <p>Implementation strategy: shells out to the platform-native keychain CLI
 * (no JNI). Linux: {@code secret-tool}; macOS: {@code security}; Windows:
 * not yet implemented.
 *
 * <p>If the platform tool is unavailable or returns no entry, callers see a
 * {@link KeychainException} — preferable to silently sending an empty password.
 */
public final class Keychain implements IKeychain {
    private static final Logger logger = LoggerFactory.getLogger(Keychain.class);

    /** Matches C++ {@code KEYCHAIN_PACKAGE} so secrets stored by C++ are visible to Java. */
    static final String PACKAGE = "lib.openapi.zserio.client";

    private static final long TIMEOUT_SECONDS = 60;

    public Keychain() {}

    @Override
    @NotNull
    public String load(@NotNull String service, @NotNull String user) {
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
        } catch (InterruptedException e) {
            // Real interruption — restore the interrupt flag so callers can react.
            Thread.currentThread().interrupt();
            throw new KeychainException("keychain: interrupted while loading secret: " + e.getMessage(), e);
        } catch (IOException e) {
            // I/O failure — do NOT touch the interrupt flag.
            throw new KeychainException("keychain: failed to load secret: " + e.getMessage(), e);
        }
    }

    private static String loadLinux(String service, String user) throws IOException, InterruptedException {
        ProcessBuilder pb = new ProcessBuilder("secret-tool", "lookup",
                "package", PACKAGE,
                "service", service,
                "user", user);
        return runReadStdout(pb, "secret-tool");
    }

    private static String loadMacOs(String service, String user) throws IOException, InterruptedException {
        ProcessBuilder pb = new ProcessBuilder("security", "find-generic-password",
                "-s", service,
                "-a", user,
                "-w");
        return runReadStdout(pb, "security").trim();
    }

    /**
     * Windows credential manager support is not yet implemented for the Java JVM client.
     * <p>
     * The C++ httpcl library wraps the C-language {@code keychain} library which handles
     * the Windows Data Protection API (DPAPI) under the hood; Python (via pyzswagcl)
     * inherits that. A Java equivalent would either shell out to {@code cmdkey}/
     * {@code vaultcmd} or call DPAPI through JNA — both are non-trivial and have been
     * scheduled for a separate follow-up.
     * <p>
     * Workaround for Windows users today: put cleartext credentials in
     * {@code http-settings.yaml} via {@code password:} (instead of {@code keychain:}),
     * or pass them adhoc through {@code HttpConfig.basicAuth(user, password)}.
     */
    private static String loadWindows(String service, String user) {
        throw new KeychainException(
                "keychain: Windows credential manager lookup is not yet implemented in the Java JVM client. "
                + "Workaround: use a cleartext 'password:' entry in http-settings.yaml, or "
                + "configure credentials adhoc via HttpConfig.basicAuth(user, password). "
                + "See README.md → Keychain integration for details. "
                + "(The C++ and Python clients DO support Windows credential manager.)");
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

}
