package com.ndsev.zswag.desktop;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.lang.reflect.Method;
import java.util.Locale;

/**
 * Wires up the {@code HTTP_LOG_LEVEL} environment variable to the SLF4J/logback
 * root logger so that running with {@code HTTP_LOG_LEVEL=debug} or
 * {@code HTTP_LOG_LEVEL=trace} produces the same diagnostics as the C++ client.
 *
 * <p>Safe to call from anywhere; idempotent. Has no effect if logback is not
 * the active SLF4J binding (e.g. on Android with a different logger).
 *
 * <p>Other env vars in scope: {@code HTTP_LOG_FILE} / {@code HTTP_LOG_FILE_MAXSIZE}
 * (rotating file appender) are NOT yet wired — see NEXT_STEPS for the gap.
 */
public final class JzswagLogging {
    private static volatile boolean initialised = false;
    private static final Object LOCK = new Object();

    private JzswagLogging() {}

    public static void init() {
        if (initialised) return;
        synchronized (LOCK) {
            if (initialised) return;
            String level = System.getenv("HTTP_LOG_LEVEL");
            if (level != null && !level.isEmpty()) {
                if (!setLogbackRootLevel(level)) {
                    // Fall back to a stderr note so the user understands why
                    // their env var didn't take effect.
                    System.err.println("[jzswag] HTTP_LOG_LEVEL=" + level
                            + " but the SLF4J binding is not logback; ignoring.");
                }
            }
            initialised = true;
        }
    }

    private static boolean setLogbackRootLevel(String levelName) {
        try {
            org.slf4j.ILoggerFactory factory = LoggerFactory.getILoggerFactory();
            // Detect logback via class name without importing it (works under any module config).
            if (!"ch.qos.logback.classic.LoggerContext".equals(factory.getClass().getName())) {
                return false;
            }
            Logger root = LoggerFactory.getLogger(Logger.ROOT_LOGGER_NAME);
            // Logback's Logger has setLevel(Level); use reflection so this class doesn't pull
            // logback into the api compile path.
            Class<?> levelClass = Class.forName("ch.qos.logback.classic.Level");
            Method toLevel = levelClass.getMethod("toLevel", String.class);
            Object level = toLevel.invoke(null, levelName.toUpperCase(Locale.ROOT));
            Class<?> logbackLogger = Class.forName("ch.qos.logback.classic.Logger");
            Method setLevel = logbackLogger.getMethod("setLevel", levelClass);
            setLevel.invoke(root, level);
            return true;
        } catch (ReflectiveOperationException | RuntimeException e) {
            return false;
        }
    }
}
