package io.github.ndsev.zswag.android;

import android.content.Context;
import android.content.SharedPreferences;
import io.github.ndsev.zswag.api.KeychainException;
import org.junit.jupiter.api.Test;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

/**
 * Plain-JUnit + Mockito tests for {@link AndroidKeychain}. Only the input
 * validation + missing-entry paths are exercised here — those don't touch
 * the platform Keystore. The encrypt/decrypt round trip and the key
 * generation path require Robolectric or an Android device, which the
 * sandbox cannot run (Conscrypt has no aarch64 Linux native).
 */
class AndroidKeychainTest {

    @Test
    void emptyServiceLoadThrows() {
        Context ctx = mock(Context.class);
        when(ctx.getApplicationContext()).thenReturn(ctx);
        AndroidKeychain kc = new AndroidKeychain(ctx);
        assertThatThrownBy(() -> kc.load("", "user"))
                .isInstanceOf(KeychainException.class)
                .hasMessageContaining("service identifier");
    }

    @Test
    void emptyServiceStoreThrows() {
        Context ctx = mock(Context.class);
        when(ctx.getApplicationContext()).thenReturn(ctx);
        AndroidKeychain kc = new AndroidKeychain(ctx);
        assertThatThrownBy(() -> kc.store("", "user", "secret"))
                .isInstanceOf(KeychainException.class);
    }

    @Test
    void loadAbsentEntryThrows() {
        Context ctx = mock(Context.class);
        when(ctx.getApplicationContext()).thenReturn(ctx);
        SharedPreferences prefs = mock(SharedPreferences.class);
        when(ctx.getSharedPreferences(eq("io.github.ndsev.zswag.keychain"), anyInt())).thenReturn(prefs);
        when(prefs.getString(eq("svc.does-not-exist|user.does-not-exist"), eq(null))).thenReturn(null);
        AndroidKeychain kc = new AndroidKeychain(ctx);
        assertThatThrownBy(() -> kc.load("svc.does-not-exist", "user.does-not-exist"))
                .isInstanceOf(KeychainException.class)
                .hasMessageContaining("no entry");
    }

    @Test
    void deleteCallsSharedPreferencesEditor() {
        Context ctx = mock(Context.class);
        when(ctx.getApplicationContext()).thenReturn(ctx);
        SharedPreferences prefs = mock(SharedPreferences.class);
        SharedPreferences.Editor editor = mock(SharedPreferences.Editor.class);
        when(prefs.edit()).thenReturn(editor);
        when(editor.remove(org.mockito.ArgumentMatchers.anyString())).thenReturn(editor);
        when(ctx.getSharedPreferences(eq("io.github.ndsev.zswag.keychain"), anyInt())).thenReturn(prefs);
        new AndroidKeychain(ctx).delete("svc", "user");
        // verifyEditing.remove was called with the joined key
        org.mockito.Mockito.verify(editor).remove("svc|user");
        org.mockito.Mockito.verify(editor).apply();
    }

    @Test
    void exceptionConstructorsPreserveMessageAndCause() {
        KeychainException simple = new KeychainException("just msg");
        assertThat(simple).hasMessage("just msg");
        Throwable cause = new RuntimeException("inner");
        KeychainException withCause = new KeychainException("outer", cause);
        assertThat(withCause).hasCause(cause).hasMessage("outer");
    }
}
