package io.github.ndsev.zswag.jvm;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.lang.reflect.Method;
import java.util.Locale;

/**
 * Wires up zswag's logging-related environment variables to the SLF4J/logback
 * root logger so the JVM client produces the same diagnostics as the C++ client.
 *
 * <ul>
 *   <li>{@code HTTP_LOG_LEVEL} — sets the root logger level (debug, trace, …).</li>
 *   <li>{@code HTTP_LOG_FILE} — adds a {@code RollingFileAppender} writing to this
 *       path. C++ uses three rotation indices ({@code FILE}, {@code FILE-1},
 *       {@code FILE-2}); we mirror that.</li>
 *   <li>{@code HTTP_LOG_FILE_MAXSIZE} — rotation size threshold in bytes
 *       (default 1 GB, matching C++ {@code log.cpp}).</li>
 * </ul>
 *
 * <p>Safe to call from anywhere; idempotent. Has no effect if logback is not
 * the active SLF4J binding (e.g. on Android with a different logger).
 */
public final class JzswagLogging {
    private static volatile boolean initialised = false;
    private static final Object LOCK = new Object();
    private static final long DEFAULT_MAX_FILE_SIZE = 1024L * 1024L * 1024L; // 1 GB, matches C++

    private JzswagLogging() {}

    public static void init() {
        if (initialised) return;
        synchronized (LOCK) {
            if (initialised) return;
            String level = System.getenv("HTTP_LOG_LEVEL");
            if (level != null && !level.isEmpty()) {
                if (!setLogbackRootLevel(level)) {
                    System.err.println("[jzswag] HTTP_LOG_LEVEL=" + level
                            + " but the SLF4J binding is not logback; ignoring.");
                }
            }
            String logFile = System.getenv("HTTP_LOG_FILE");
            if (logFile != null && !logFile.isEmpty()) {
                long maxSize = parseMaxSize(System.getenv("HTTP_LOG_FILE_MAXSIZE"));
                if (!attachLogbackFileAppender(logFile, maxSize)) {
                    System.err.println("[jzswag] HTTP_LOG_FILE=" + logFile
                            + " but the SLF4J binding is not logback; file logging ignored.");
                }
            }
            initialised = true;
        }
    }

    private static long parseMaxSize(String env) {
        if (env == null || env.isEmpty()) return DEFAULT_MAX_FILE_SIZE;
        try {
            return Long.parseLong(env.trim());
        } catch (NumberFormatException e) {
            System.err.println("[jzswag] Invalid HTTP_LOG_FILE_MAXSIZE='" + env
                    + "', using default " + DEFAULT_MAX_FILE_SIZE + " bytes.");
            return DEFAULT_MAX_FILE_SIZE;
        }
    }

    private static boolean setLogbackRootLevel(String levelName) {
        try {
            org.slf4j.ILoggerFactory factory = LoggerFactory.getILoggerFactory();
            if (!"ch.qos.logback.classic.LoggerContext".equals(factory.getClass().getName())) {
                return false;
            }
            Logger root = LoggerFactory.getLogger(Logger.ROOT_LOGGER_NAME);
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

    /**
     * Builds a {@code RollingFileAppender} with a {@code FixedWindowRollingPolicy}
     * (3-file window: FILE, FILE-1, FILE-2) and a {@code SizeBasedTriggeringPolicy}.
     * Mirrors the C++ {@code log.cpp} setup. All wiring is done reflectively so this
     * class doesn't compile-time-depend on logback (the api/shared modules don't either).
     */
    private static boolean attachLogbackFileAppender(String logFile, long maxFileSizeBytes) {
        try {
            org.slf4j.ILoggerFactory factory = LoggerFactory.getILoggerFactory();
            if (!"ch.qos.logback.classic.LoggerContext".equals(factory.getClass().getName())) {
                return false;
            }
            // Pattern matches the typical logback default — match cpp's log line layout
            // enough that grep across language logs is feasible.
            String pattern = "%d{yyyy-MM-dd HH:mm:ss.SSS} [%thread] %-5level %logger{36} - %msg%n";

            // PatternLayoutEncoder
            Class<?> peClass = Class.forName("ch.qos.logback.classic.encoder.PatternLayoutEncoder");
            Object encoder = peClass.getDeclaredConstructor().newInstance();
            peClass.getMethod("setContext", Class.forName("ch.qos.logback.core.Context"))
                    .invoke(encoder, factory);
            peClass.getMethod("setPattern", String.class).invoke(encoder, pattern);
            peClass.getMethod("start").invoke(encoder);

            // FixedWindowRollingPolicy — 3-file window FILE / FILE-1 / FILE-2
            Class<?> rpClass = Class.forName("ch.qos.logback.core.rolling.FixedWindowRollingPolicy");
            Object rollingPolicy = rpClass.getDeclaredConstructor().newInstance();
            rpClass.getMethod("setContext", Class.forName("ch.qos.logback.core.Context"))
                    .invoke(rollingPolicy, factory);
            rpClass.getMethod("setFileNamePattern", String.class)
                    .invoke(rollingPolicy, logFile + "-%i");
            rpClass.getMethod("setMinIndex", int.class).invoke(rollingPolicy, 1);
            rpClass.getMethod("setMaxIndex", int.class).invoke(rollingPolicy, 2);

            // SizeBasedTriggeringPolicy
            Class<?> tpClass = Class.forName("ch.qos.logback.core.rolling.SizeBasedTriggeringPolicy");
            Object triggeringPolicy = tpClass.getDeclaredConstructor().newInstance();
            tpClass.getMethod("setContext", Class.forName("ch.qos.logback.core.Context"))
                    .invoke(triggeringPolicy, factory);
            // FileSize.valueOf accepts strings like "1GB"; using a raw byte count via toString.
            Class<?> fileSizeClass = Class.forName("ch.qos.logback.core.util.FileSize");
            Method fileSizeValueOf = fileSizeClass.getMethod("valueOf", String.class);
            Object fileSize = fileSizeValueOf.invoke(null, maxFileSizeBytes + "");
            tpClass.getMethod("setMaxFileSize", fileSizeClass).invoke(triggeringPolicy, fileSize);
            tpClass.getMethod("start").invoke(triggeringPolicy);

            // RollingFileAppender
            Class<?> rfaClass = Class.forName("ch.qos.logback.core.rolling.RollingFileAppender");
            Object appender = rfaClass.getDeclaredConstructor().newInstance();
            rfaClass.getMethod("setContext", Class.forName("ch.qos.logback.core.Context"))
                    .invoke(appender, factory);
            rfaClass.getMethod("setName", String.class).invoke(appender, "jzswag-http-log-file");
            rfaClass.getMethod("setFile", String.class).invoke(appender, logFile);
            rfaClass.getMethod("setEncoder", Class.forName("ch.qos.logback.core.encoder.Encoder"))
                    .invoke(appender, encoder);
            // Hook the rolling/triggering policies onto the appender + each other.
            rfaClass.getMethod("setRollingPolicy",
                    Class.forName("ch.qos.logback.core.rolling.RollingPolicy"))
                    .invoke(appender, rollingPolicy);
            rfaClass.getMethod("setTriggeringPolicy",
                    Class.forName("ch.qos.logback.core.rolling.TriggeringPolicy"))
                    .invoke(appender, triggeringPolicy);
            // setParent on rollingPolicy needs the appender — order matters.
            rpClass.getMethod("setParent",
                    Class.forName("ch.qos.logback.core.FileAppender"))
                    .invoke(rollingPolicy, appender);
            rpClass.getMethod("start").invoke(rollingPolicy);
            rfaClass.getMethod("start").invoke(appender);

            // Attach to root logger.
            Logger root = LoggerFactory.getLogger(Logger.ROOT_LOGGER_NAME);
            Class<?> logbackLogger = Class.forName("ch.qos.logback.classic.Logger");
            Method addAppender = logbackLogger.getMethod("addAppender",
                    Class.forName("ch.qos.logback.core.Appender"));
            addAppender.invoke(root, appender);
            return true;
        } catch (ReflectiveOperationException | RuntimeException e) {
            System.err.println("[jzswag] Failed to install HTTP_LOG_FILE appender: " + e.getMessage());
            return false;
        }
    }
}
