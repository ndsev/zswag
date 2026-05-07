package io.github.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;

/**
 * Platform-agnostic keychain abstraction. Loads a stored password for
 * {@code (service, user)} from the host's secure credential store.
 *
 * <p>Implementations live in the platform modules: {@code jzswag-jvm} shells
 * out to {@code secret-tool} (Linux) / {@code security} (macOS); {@code
 * jzswag-android} uses the Android Keystore (AES-256-GCM master key in the
 * platform secure enclave) to encrypt entries stored in a private
 * {@code SharedPreferences} file.
 *
 * <p>Implementations should throw an unchecked exception if the platform tool
 * is missing or the entry doesn't exist — preferable to silently sending an
 * empty password.
 */
public interface IKeychain {
    /**
     * Loads a stored password for {@code (service, user)}. Throws if the
     * platform store is unreachable or the entry doesn't exist.
     */
    @NotNull
    String load(@NotNull String service, @NotNull String user);
}
