package io.github.ndsev.zswag.android;

import org.junit.jupiter.api.Test;

import java.lang.reflect.Field;

import static org.assertj.core.api.Assertions.assertThatCode;

/**
 * Plain-JUnit smoke tests for {@link AndroidLogging}. Only the env-var-unset
 * path is exercised here — that path doesn't hit android.util.Log so it works
 * on plain JVM. Tests that exercise the env-var-set branch (which routes
 * through android.util.Log) need a device or x86_64 host with Robolectric.
 */
class AndroidLoggingTest {

    private void resetInitialised() throws Exception {
        Field f = AndroidLogging.class.getDeclaredField("initialised");
        f.setAccessible(true);
        f.set(null, false);
    }

    @Test
    void initIsIdempotent() {
        AndroidLogging.init();
        AndroidLogging.init();
    }

    @Test
    void initWithoutEnvVarDoesNotThrow() throws Exception {
        // HTTP_LOG_LEVEL is not set in the JUnit env, so init() takes the
        // null-level branch (no Log.d call) — safe to run on plain JVM.
        resetInitialised();
        assertThatCode(AndroidLogging::init).doesNotThrowAnyException();
    }
}
