package io.github.ndsev.zswag.api;

/**
 * Thrown by {@link IKeychain} implementations when a stored secret cannot be
 * loaded — the platform tool is missing, the entry doesn't exist, the user
 * cancelled an unlock prompt, etc. Lives in the platform-agnostic API module
 * so cross-platform consumers can catch a single stable type rather than the
 * platform-specific {@code Keychain.KeychainException} or
 * {@code AndroidKeychain.KeychainException}.
 */
public class KeychainException extends RuntimeException {
    public KeychainException(String message) {
        super(message);
    }

    public KeychainException(String message, Throwable cause) {
        super(message, cause);
    }
}
