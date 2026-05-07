package io.github.ndsev.zswag.shared;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import zserio.runtime.io.BitStreamWriter;
import zserio.runtime.io.ByteArrayBitStreamWriter;
import zserio.runtime.io.Writer;

import java.io.IOException;
import java.lang.reflect.Method;

/**
 * Resolves zswag {@code x-zserio-request-part} dotted paths against typed
 * zserio request objects via JavaBean-style getter reflection. Mirrors the
 * Python/C++ "find by member path" flow ({@code reflectable->find(field)}
 * in {@code oaclient.cpp:141}) but uses Java's POJO accessor convention
 * because zserio Java codegen does not emit a runtime introspection view.
 *
 * <p>Path semantics:
 * <ul>
 *   <li>{@code "*"} means "the whole request object" — caller serializes it.</li>
 *   <li>Empty path means "the whole request object" — same as {@code "*"}.</li>
 *   <li>Dot-separated segments are zserio identifiers (snake_case allowed);
 *       each is normalized to lowerCamel and looked up via {@code getXxx()}.</li>
 *   <li>Zserio enum values are unwrapped to their underlying numeric via
 *       {@code ZserioEnum.getGenericValue()} so they can be encoded via the
 *       OpenAPI parameter format (string/hex/base64/...).</li>
 * </ul>
 *
 * <p>Resolution returns the raw Java value (primitive box, array, byte[],
 * String, or another zserio compound). The caller decides how to encode it.
 */
public final class ZserioReflection {

    private ZserioReflection() {}

    /**
     * Resolves the given path against {@code root}.
     * Returns {@code null} only when an intermediate segment evaluates to {@code null};
     * use {@link #resolveOptional} for caller-controlled handling.
     *
     * @throws IllegalArgumentException if a path segment doesn't correspond to a getter.
     */
    @Nullable
    public static Object resolve(@NotNull Object root, @NotNull String dottedPath) {
        if (dottedPath.isEmpty() || "*".equals(dottedPath)) {
            return root;
        }
        Object current = root;
        for (String segment : dottedPath.split("\\.")) {
            if (current == null) {
                return null;
            }
            current = invokeGetter(current, segment);
        }
        // Unwrap zserio enum to its underlying numeric.
        if (current instanceof zserio.runtime.ZserioEnum) {
            return ((zserio.runtime.ZserioEnum) current).getGenericValue();
        }
        return current;
    }

    /**
     * Serializes a zserio {@link Writer} (any zserio struct/choice/union/etc.)
     * to a byte array using the standard bitstream writer. Mirrors the
     * {@code writeAll} step in {@code OAClient::callMethod} for whole-blob bodies.
     */
    @NotNull
    public static byte[] serialize(@NotNull Writer obj) {
        try (ByteArrayBitStreamWriter w = new ByteArrayBitStreamWriter()) {
            obj.write(w);
            return w.toByteArray();
        } catch (IOException e) {
            throw new IllegalStateException("Failed to serialize zserio object: " + e.getMessage(), e);
        }
    }

    /**
     * Convenience: resolves the path and, if the result is a zserio
     * {@link Writer}, serializes it to bytes (i.e. nested-compound case at
     * the same code path as {@code reflectableToParameterValue}'s
     * {@code STRUCT/CHOICE/UNION} branch in {@code oaclient.cpp:97-112}).
     */
    @Nullable
    public static Object resolveOrSerialize(@NotNull Object root, @NotNull String dottedPath) {
        Object resolved = resolve(root, dottedPath);
        if (resolved instanceof Writer && !"*".equals(dottedPath) && !dottedPath.isEmpty()) {
            return serialize((Writer) resolved);
        }
        return resolved;
    }

    /**
     * Calls the JavaBean getter for the given zserio identifier on {@code obj}.
     * Tries {@code getX} first, then {@code isX} for booleans.
     */
    @NotNull
    private static Object invokeGetter(@NotNull Object obj, @NotNull String zserioIdent) {
        String getter = toGetterName(zserioIdent, "get");
        Class<?> cls = obj.getClass();
        Method m = findNoArgMethod(cls, getter);
        if (m == null) {
            String isGetter = toGetterName(zserioIdent, "is");
            m = findNoArgMethod(cls, isGetter);
        }
        if (m == null) {
            throw new IllegalArgumentException(
                    "No getter for zserio field '" + zserioIdent + "' on " + cls.getSimpleName()
                            + " (tried " + getter + "() and " + toGetterName(zserioIdent, "is") + "())");
        }
        try {
            Object result = m.invoke(obj);
            if (result == null) {
                throw new IllegalStateException(
                        "Getter " + cls.getSimpleName() + "." + m.getName() + "() returned null while resolving zserio path");
            }
            return result;
        } catch (ReflectiveOperationException e) {
            throw new IllegalStateException(
                    "Failed to invoke " + cls.getSimpleName() + "." + m.getName() + "(): " + e.getMessage(), e);
        }
    }

    @Nullable
    private static Method findNoArgMethod(@NotNull Class<?> cls, @NotNull String name) {
        try {
            return cls.getMethod(name);
        } catch (NoSuchMethodException e) {
            return null;
        }
    }

    /**
     * Converts a zserio identifier ({@code base}, {@code enum_value}, {@code my_field_2}) to
     * its corresponding JavaBean getter name with the given prefix
     * ({@code "get"} or {@code "is"}). Snake_case underscores mark word boundaries
     * (each next word starts capitalized); other characters pass through.
     */
    @NotNull
    static String toGetterName(@NotNull String zserioIdent, @NotNull String prefix) {
        StringBuilder out = new StringBuilder(prefix);
        boolean nextUpper = true;
        for (int i = 0; i < zserioIdent.length(); i++) {
            char c = zserioIdent.charAt(i);
            if (c == '_') {
                nextUpper = true;
                continue;
            }
            out.append(nextUpper ? Character.toUpperCase(c) : c);
            nextUpper = false;
        }
        return out.toString();
    }
}
