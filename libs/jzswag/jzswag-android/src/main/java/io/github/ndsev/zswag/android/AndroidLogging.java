package io.github.ndsev.zswag.android;

import android.util.Log;

/**
 * Android equivalent of the JVM's {@code JzswagLogging}. On Android the SLF4J
 * binding ({@code uk.uuid:slf4j-android}) routes through {@link Log}, whose
 * tag-level filtering is set by the platform (logcat / {@code setprop
 * log.tag.<TAG> <LEVEL>}) rather than by the application.
 *
 * <p>Therefore there is no programmatic root-level change to perform: this
 * class exists so app code can call {@link #init()} symmetrically with the
 * JVM port, but the call is a near-noop. If {@code HTTP_LOG_LEVEL} is set in
 * the process environment, we surface it to logcat once at debug level so
 * the developer can confirm the value the JVM modules would have used.
 */
public final class AndroidLogging {
    private static volatile boolean initialised = false;
    private static final Object LOCK = new Object();
    private static final String TAG = "jzswag";

    private AndroidLogging() {}

    public static void init() {
        if (initialised) return;
        synchronized (LOCK) {
            if (initialised) return;
            String level = System.getenv("HTTP_LOG_LEVEL");
            if (level != null && !level.isEmpty()) {
                Log.d(TAG, "HTTP_LOG_LEVEL=" + level + " observed in environment. "
                        + "On Android, log filtering is controlled by logcat tag levels "
                        + "(setprop log.tag." + TAG + " " + level.toUpperCase() + ")");
            }
            initialised = true;
        }
    }
}
