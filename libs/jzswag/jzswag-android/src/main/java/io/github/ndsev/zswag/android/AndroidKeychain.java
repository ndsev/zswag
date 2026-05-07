package io.github.ndsev.zswag.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import io.github.ndsev.zswag.api.IKeychain;
import org.jetbrains.annotations.NotNull;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;
import java.util.Base64;

/**
 * Android keychain integration using the platform Keystore. Mirrors the
 * {@link IKeychain} contract that the JVM {@code Keychain} class implements,
 * so {@code OAuth2Handler} and {@code AndroidHttpClient} consume both
 * interchangeably via dependency injection.
 *
 * <p>Storage strategy:
 * <ul>
 *   <li>A symmetric AES-256-GCM key is generated in the platform Keystore on
 *       first use, aliased {@code io.github.ndsev.zswag.keychain.master}.
 *       The key never leaves the secure hardware (TEE / StrongBox where
 *       available); only a {@link Cipher} handle does.</li>
 *   <li>Per-credential entries (one per {@code service|user} pair) are
 *       encrypted with that key and stored in a private
 *       {@link SharedPreferences} file
 *       ({@code io.github.ndsev.zswag.keychain}). The on-disk blob is
 *       {@code base64(iv_len:byte | iv | ciphertext_with_gcm_tag)}.</li>
 * </ul>
 *
 * <p>Why not {@code androidx.security:security-crypto}? That library is
 * distributed as an AAR which the {@code java-library}-based build of this
 * module cannot consume (see this module's build.gradle for the aapt2-on-arm
 * trade-off). Doing the AES/GCM dance manually keeps us inside Java APIs
 * that work both at compile time (against the Robolectric android.jar) and
 * at runtime (on a real device).
 *
 * <p>Storage of new secrets is a programmatic operation
 * ({@link #store(String, String, String)}); zswag itself only ever
 * <em>reads</em> via {@link IKeychain#load} so writes are typically issued
 * out-of-band by the host app.
 */
public final class AndroidKeychain implements IKeychain {
    private static final Logger logger = LoggerFactory.getLogger(AndroidKeychain.class);

    /** Matches the JVM keychain package id so credentials stored on a JVM laptop and synced to a device line up. */
    static final String KEYSTORE_TYPE = "AndroidKeyStore";
    static final String KEY_ALIAS = "io.github.ndsev.zswag.keychain.master";
    static final String PREFS_NAME = "io.github.ndsev.zswag.keychain";
    private static final int GCM_TAG_BITS = 128;

    private final Context appContext;

    public AndroidKeychain(@NotNull Context context) {
        this.appContext = context.getApplicationContext();
    }

    @Override
    @NotNull
    public String load(@NotNull String service, @NotNull String user) {
        if (service.isEmpty()) {
            throw new KeychainException("keychain: service identifier must not be empty");
        }
        SharedPreferences prefs = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        String key = entryKey(service, user);
        String encoded = prefs.getString(key, null);
        if (encoded == null) {
            throw new KeychainException("keychain: no entry for service='" + service + "' user='" + user + "'");
        }
        try {
            return decrypt(encoded);
        } catch (Exception e) {
            throw new KeychainException("keychain: failed to decrypt entry for '" + key + "': " + e.getMessage(), e);
        }
    }

    /**
     * Stores or overwrites a credential under {@code (service, user)}. Apps
     * typically call this once at first-run during their auth onboarding;
     * zswag itself never writes.
     */
    public void store(@NotNull String service, @NotNull String user, @NotNull String secret) {
        if (service.isEmpty()) {
            throw new KeychainException("keychain: service identifier must not be empty");
        }
        try {
            String encrypted = encrypt(secret);
            appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                    .edit()
                    .putString(entryKey(service, user), encrypted)
                    .apply();
            logger.debug("Stored keychain entry for service='{}' user='{}'", service, user);
        } catch (Exception e) {
            throw new KeychainException("keychain: failed to encrypt entry: " + e.getMessage(), e);
        }
    }

    /** Removes the credential under {@code (service, user)} if present. */
    public void delete(@NotNull String service, @NotNull String user) {
        appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .edit()
                .remove(entryKey(service, user))
                .apply();
    }

    @NotNull
    private static String entryKey(@NotNull String service, @NotNull String user) {
        return service + "|" + user;
    }

    @NotNull
    private SecretKey getOrCreateMasterKey() throws Exception {
        KeyStore ks = KeyStore.getInstance(KEYSTORE_TYPE);
        ks.load(null);
        if (ks.containsAlias(KEY_ALIAS)) {
            KeyStore.Entry entry = ks.getEntry(KEY_ALIAS, null);
            if (entry instanceof KeyStore.SecretKeyEntry) {
                return ((KeyStore.SecretKeyEntry) entry).getSecretKey();
            }
            throw new KeychainException("keychain: unexpected entry type for alias " + KEY_ALIAS);
        }
        KeyGenerator kg = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE_TYPE);
        kg.init(new KeyGenParameterSpec.Builder(
                KEY_ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build());
        return kg.generateKey();
    }

    @NotNull
    private String encrypt(@NotNull String plaintext) throws Exception {
        SecretKey key = getOrCreateMasterKey();
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, key);
        byte[] iv = cipher.getIV();
        byte[] ct = cipher.doFinal(plaintext.getBytes(StandardCharsets.UTF_8));
        ByteBuffer buf = ByteBuffer.allocate(1 + iv.length + ct.length);
        buf.put((byte) iv.length).put(iv).put(ct);
        return Base64.getEncoder().encodeToString(buf.array());
    }

    @NotNull
    private String decrypt(@NotNull String encoded) throws Exception {
        byte[] packed = Base64.getDecoder().decode(encoded);
        int ivLen = packed[0] & 0xff;
        byte[] iv = Arrays.copyOfRange(packed, 1, 1 + ivLen);
        byte[] ct = Arrays.copyOfRange(packed, 1 + ivLen, packed.length);
        SecretKey key = getOrCreateMasterKey();
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, key, new GCMParameterSpec(GCM_TAG_BITS, iv));
        return new String(cipher.doFinal(ct), StandardCharsets.UTF_8);
    }

    /** Thrown when a keychain operation fails. */
    public static class KeychainException extends RuntimeException {
        public KeychainException(String message) { super(message); }
        public KeychainException(String message, Throwable cause) { super(message, cause); }
    }
}
