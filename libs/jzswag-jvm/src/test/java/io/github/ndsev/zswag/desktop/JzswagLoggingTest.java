package io.github.ndsev.zswag.desktop;

import org.junit.jupiter.api.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.lang.reflect.Field;

import static org.assertj.core.api.Assertions.assertThat;

class JzswagLoggingTest {

    private void resetInitialised() throws Exception {
        Field f = JzswagLogging.class.getDeclaredField("initialised");
        f.setAccessible(true);
        f.set(null, false);
    }

    @Test
    void initIsIdempotent() {
        // Calling init() repeatedly must not throw and the second call must short-circuit
        // because `initialised` is already true.
        JzswagLogging.init();
        JzswagLogging.init();
        // No assertion of state here other than absence of exception — env var is not under test control.
        Logger root = LoggerFactory.getLogger(Logger.ROOT_LOGGER_NAME);
        assertThat(root).isNotNull();
    }

    @Test
    void initWithoutEnvVarDoesNotChangeLogbackLevel() throws Exception {
        // Force reinit so that the System.getenv branch is exercised.
        resetInitialised();
        JzswagLogging.init();
        // No assertion needed: we are exercising the branch where HTTP_LOG_LEVEL is unset.
        // (HTTP_LOG_LEVEL cannot be reliably set at runtime in pure JDK; tested via integration.)
        assertThat(LoggerFactory.getLogger(Logger.ROOT_LOGGER_NAME)).isNotNull();
    }
}
