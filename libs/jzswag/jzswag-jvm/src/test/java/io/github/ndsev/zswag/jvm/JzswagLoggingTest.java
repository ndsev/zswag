package io.github.ndsev.zswag.jvm;

import org.junit.jupiter.api.Test;

import java.lang.reflect.Field;

import static org.assertj.core.api.Assertions.assertThatCode;

/**
 * Smoke tests for {@link JzswagLogging}. The full HTTP_LOG_LEVEL → logback
 * root-logger plumbing isn't testable in pure JUnit — env vars can't reliably
 * be set at runtime. We verify that {@code init()} is idempotent and doesn't
 * throw on the env-var-unset branch (the typical CI path).
 */
class JzswagLoggingTest {

    private void resetInitialised() throws Exception {
        Field f = JzswagLogging.class.getDeclaredField("initialised");
        f.setAccessible(true);
        f.set(null, false);
    }

    @Test
    void initIsIdempotent() {
        assertThatCode(() -> {
            JzswagLogging.init();
            JzswagLogging.init();
        }).doesNotThrowAnyException();
    }

    @Test
    void initWithoutEnvVarDoesNotThrow() throws Exception {
        resetInitialised();
        assertThatCode(JzswagLogging::init).doesNotThrowAnyException();
    }
}
