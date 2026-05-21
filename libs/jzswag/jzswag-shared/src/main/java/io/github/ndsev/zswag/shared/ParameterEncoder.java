package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.StringJoiner;

/**
 * Encodes OpenAPI parameter values for path, query, header, and cookie locations.
 * Mirrors the C++ {@code openapi-parameter-helper.cpp} contract:
 *
 * <ul>
 *   <li>Path/header/cookie locations return one styled string per parameter.</li>
 *   <li>Query parameters return a list of {@code (name, value)} pairs so that
 *       {@code style: form, explode: true} can yield repeated query keys
 *       ({@code ?id=1&id=2&id=3}).</li>
 *   <li>Whole-blob body parameters ({@code x-zserio-request-part: "*"}) are
 *       served as raw bytes via {@link #encodeWholeBlobBody}.</li>
 * </ul>
 */
public class ParameterEncoder {

    private ParameterEncoder() {}

    // ------------------------------------------------------------------------
    // High-level API: encode for a specific location.
    // ------------------------------------------------------------------------

    /**
     * Encodes a parameter for a path placeholder (or label/matrix style on a
     * path param).
     */
    @NotNull
    public static String encodeForPath(@NotNull OpenAPIParameter param, @NotNull Object value) {
        List<String> arrayElements = formatArrayElements(value, param.getFormat());
        if (arrayElements != null) {
            return applyPathArrayStyle(param.getName(), arrayElements, param.getStyle(), param.isExplode());
        }
        if (value instanceof Map) {
            return applyPathMapStyle(param.getName(),
                    (Map<?, ?>) value, param.getStyle(), param.isExplode(), param.getFormat());
        }
        String formatted = formatScalarValue(value, param.getFormat());
        return applyPathScalarStyle(param.getName(), formatted, param.getStyle());
    }

    /**
     * Encodes a parameter for a header value. Style is always {@code simple} for
     * headers per OpenAPI; {@code explode} only matters for arrays.
     */
    @NotNull
    public static String encodeForHeader(@NotNull OpenAPIParameter param, @NotNull Object value) {
        List<String> arrayElements = formatArrayElements(value, param.getFormat());
        if (arrayElements != null) {
            // simple style: comma-joined
            return String.join(",", arrayElements);
        }
        if (value instanceof Map) {
            // simple style on map: "k1,v1,k2,v2" (no explode in header per OpenAPI).
            return String.join(",", flattenMapForJoin((Map<?, ?>) value, param.getFormat()));
        }
        return formatScalarValue(value, param.getFormat());
    }

    /**
     * Encodes a parameter for a cookie. Returns just the cookie value; the
     * caller assembles {@code name=value; …} into the {@code Cookie} header.
     */
    @NotNull
    public static String encodeForCookie(@NotNull OpenAPIParameter param, @NotNull Object value) {
        List<String> arrayElements = formatArrayElements(value, param.getFormat());
        if (arrayElements != null) {
            // form style, comma-joined when not exploded; explode + cookie isn't well-defined.
            return String.join(",", arrayElements);
        }
        if (value instanceof Map) {
            // Cookie carrying a compound/map value: comma-joined "k1,v1,k2,v2" (matches the
            // C++ openapi-parameter-helper encodeForCookie of a map).
            return String.join(",", flattenMapForJoin((Map<?, ?>) value, param.getFormat()));
        }
        return formatScalarValue(value, param.getFormat());
    }

    /**
     * Encodes a parameter for the query string. Returns a list of
     * {@code (name, value)} pairs. For {@code style: form, explode: true}
     * arrays this is one pair per element; for {@code explode: false} arrays
     * it's a single pair with comma-joined values; scalars are a single pair.
     *
     * <p>Map-shaped values (e.g. a zserio compound resolved through a future
     * IReflectableView): explode=true emits one pair per map entry
     * ({@code ?k1=v1&k2=v2}); explode=false emits a single comma-joined pair
     * ({@code ?name=k1,v1,k2,v2}) — matches C++ {@code queryOrHeaderPairs} at
     * openapi-parameter-helper.cpp:197-205.
     */
    @NotNull
    public static List<Map.Entry<String, String>> encodeForQuery(@NotNull OpenAPIParameter param, @NotNull Object value) {
        List<Map.Entry<String, String>> result = new ArrayList<>();
        List<String> arrayElements = formatArrayElements(value, param.getFormat());
        if (arrayElements != null) {
            if (param.isExplode()) {
                for (String v : arrayElements) {
                    result.add(new AbstractMap.SimpleImmutableEntry<>(param.getName(), v));
                }
            } else {
                result.add(new AbstractMap.SimpleImmutableEntry<>(param.getName(), String.join(",", arrayElements)));
            }
            return result;
        }
        if (value instanceof Map) {
            Map<?, ?> map = (Map<?, ?>) value;
            if (param.isExplode()) {
                // ?k1=v1&k2=v2 — each map entry becomes its own pair.
                for (Map.Entry<?, ?> e : map.entrySet()) {
                    result.add(new AbstractMap.SimpleImmutableEntry<>(
                            String.valueOf(e.getKey()),
                            formatScalarValue(e.getValue(), param.getFormat())));
                }
            } else {
                // ?paramName=k1,v1,k2,v2 — flattened, single-pair form.
                result.add(new AbstractMap.SimpleImmutableEntry<>(
                        param.getName(),
                        String.join(",", flattenMapForJoin(map, param.getFormat()))));
            }
            return result;
        }
        String formatted = formatScalarValue(value, param.getFormat());
        result.add(new AbstractMap.SimpleImmutableEntry<>(param.getName(), formatted));
        return result;
    }

    /** Helper: flatten a Map's entries to ["k1","v1","k2","v2", ...] using the given format. */
    @NotNull
    private static List<String> flattenMapForJoin(@NotNull Map<?, ?> map, @NotNull ParameterFormat format) {
        List<String> flat = new ArrayList<>(map.size() * 2);
        for (Map.Entry<?, ?> e : map.entrySet()) {
            flat.add(String.valueOf(e.getKey()));
            flat.add(formatScalarValue(e.getValue(), format));
        }
        return flat;
    }

    /**
     * Returns the raw bytes of {@code value} for use as an
     * {@code application/x-zserio-object} request body. Used when
     * {@code x-zserio-request-part: "*"} is in effect.
     */
    @NotNull
    public static byte[] encodeWholeBlobBody(@NotNull Object value) {
        if (value instanceof byte[]) return (byte[]) value;
        return toBytes(value);
    }

    // ------------------------------------------------------------------------
    // Style application — split by location to mirror C++ helper.
    // ------------------------------------------------------------------------

    @NotNull
    private static String applyPathScalarStyle(@NotNull String name, @NotNull String value,
                                               @NotNull ParameterStyle style) {
        switch (style) {
            case SIMPLE: return value;
            case LABEL:  return "." + value;
            case MATRIX: return ";" + name + "=" + value;
            default:     return value;
        }
    }

    /**
     * Path-style application for map-shaped values. Matches C++
     * {@code openapi-parameter-helper::pathStr} on a map (openapi-parameter-helper.cpp:140-160).
     *
     * <p>Style × explode behaviour:
     * <ul>
     *   <li>{@code simple} (any explode): {@code k1,v1,k2,v2} or
     *       {@code k1=v1,k2=v2} when explode (per OpenAPI 3 spec).</li>
     *   <li>{@code label}:  {@code .k1.v1.k2.v2} or {@code .k1=v1.k2=v2} (explode).</li>
     *   <li>{@code matrix}: {@code ;name=k1,v1,k2,v2} or {@code ;k1=v1;k2=v2} (explode).</li>
     * </ul>
     */
    @NotNull
    private static String applyPathMapStyle(@NotNull String name, @NotNull Map<?, ?> map,
                                            @NotNull ParameterStyle style, boolean explode,
                                            @NotNull ParameterFormat format) {
        if (map.isEmpty()) return "";
        switch (style) {
            case SIMPLE:
                return joinMapEntries(map, explode ? "=" : ",", ",", format);
            case LABEL:
                return "." + joinMapEntries(map, explode ? "=" : ",", explode ? "." : ",", format);
            case MATRIX:
                if (explode) {
                    StringBuilder sb = new StringBuilder();
                    for (Map.Entry<?, ?> e : map.entrySet()) {
                        sb.append(';').append(e.getKey()).append('=')
                                .append(formatScalarValue(e.getValue(), format));
                    }
                    return sb.toString();
                }
                return ";" + name + "=" + String.join(",", flattenMapForJoin(map, format));
            default:
                return String.join(",", flattenMapForJoin(map, format));
        }
    }

    /**
     * Helper: render a map as a list of {@code key<kvSep>value} pairs joined with
     * {@code entrySep}.
     */
    @NotNull
    private static String joinMapEntries(@NotNull Map<?, ?> map, @NotNull String kvSep,
                                         @NotNull String entrySep, @NotNull ParameterFormat format) {
        StringBuilder sb = new StringBuilder();
        boolean first = true;
        for (Map.Entry<?, ?> e : map.entrySet()) {
            if (!first) sb.append(entrySep);
            sb.append(e.getKey()).append(kvSep).append(formatScalarValue(e.getValue(), format));
            first = false;
        }
        return sb.toString();
    }

    @NotNull
    private static String applyPathArrayStyle(@NotNull String name, @NotNull List<String> values,
                                              @NotNull ParameterStyle style, boolean explode) {
        if (values.isEmpty()) return "";
        switch (style) {
            case SIMPLE:
                return String.join(",", values);
            case LABEL:
                return "." + (explode ? String.join(".", values) : String.join(",", values));
            case MATRIX:
                if (explode) {
                    StringBuilder sb = new StringBuilder();
                    for (String v : values) sb.append(';').append(name).append('=').append(v);
                    return sb.toString();
                }
                return ";" + name + "=" + String.join(",", values);
            default:
                return String.join(",", values);
        }
    }

    // ------------------------------------------------------------------------
    // Format conversion (scalar & array elements).
    // ------------------------------------------------------------------------

    @Nullable
    private static List<String> formatArrayElements(@NotNull Object value, @NotNull ParameterFormat format) {
        if (value instanceof Collection) {
            List<String> result = new ArrayList<>();
            for (Object element : (Collection<?>) value) result.add(formatScalarValue(element, format));
            return result;
        } else if (value instanceof Object[]) {
            List<String> result = new ArrayList<>();
            for (Object element : (Object[]) value) result.add(formatScalarValue(element, format));
            return result;
        } else if (value instanceof short[]) {
            short[] arr = (short[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (short v : arr) result.add(formatWithByteWidth(v, 1, format));
            return result;
        } else if (value instanceof int[]) {
            int[] arr = (int[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (int v : arr) result.add(formatWithByteWidth(v, 4, format));
            return result;
        } else if (value instanceof long[]) {
            long[] arr = (long[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (long v : arr) result.add(formatWithByteWidth(v, 8, format));
            return result;
        } else if (value instanceof double[]) {
            double[] arr = (double[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (double v : arr) result.add(formatScalarValue(v, format));
            return result;
        } else if (value instanceof float[]) {
            float[] arr = (float[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (float v : arr) result.add(formatScalarValue(v, format));
            return result;
        } else if (value instanceof boolean[]) {
            boolean[] arr = (boolean[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (boolean v : arr) result.add(formatScalarValue(v, format));
            return result;
        }
        // byte[] is binary scalar, not array.
        return null;
    }

    @NotNull
    private static String formatWithByteWidth(long value, int byteWidth, @NotNull ParameterFormat format) {
        switch (format) {
            case STRING:    return String.valueOf(value);
            case HEX:       return toSignedHexString(value);
            case BASE64:    return Base64.getEncoder().encodeToString(toBytesWithWidth(value, byteWidth));
            case BASE64URL: return Base64.getUrlEncoder().encodeToString(toBytesWithWidth(value, byteWidth));
            case BINARY:    return String.valueOf(value);
            default:        return String.valueOf(value);
        }
    }

    @NotNull
    private static String formatScalarValue(@NotNull Object value, @NotNull ParameterFormat format) {
        // Booleans: "0" / "1" (server-side parsing matches C++ behavior).
        if (value instanceof Boolean) return ((Boolean) value) ? "1" : "0";

        switch (format) {
            case STRING:    return String.valueOf(value);
            case HEX:       return toHexString(value);
            case BASE64:    return Base64.getEncoder().encodeToString(toBytes(value));
            case BASE64URL: return Base64.getUrlEncoder().encodeToString(toBytes(value));
            case BINARY:
                // Binary == raw bytes interpreted as a string per C++ formatBuffer. For numeric
                // types this is rarely meaningful; for byte[]/String it's the identity.
                return value instanceof byte[]
                        ? new String((byte[]) value, StandardCharsets.UTF_8)
                        : String.valueOf(value);
            default:        return String.valueOf(value);
        }
    }

    @NotNull
    private static String toSignedHexString(long value) {
        return value < 0 ? "-" + Long.toHexString(-value) : Long.toHexString(value);
    }

    @NotNull
    private static String toHexString(@NotNull Object value) {
        if (value instanceof byte[]) {
            byte[] bytes = (byte[]) value;
            StringBuilder hex = new StringBuilder();
            for (byte b : bytes) hex.append(String.format("%02x", b & 0xFF));
            return hex.toString();
        } else if (value instanceof Number) {
            return toSignedHexString(((Number) value).longValue());
        }
        return String.valueOf(value);
    }

    @NotNull
    private static byte[] toBytes(@NotNull Object value) {
        if (value instanceof byte[]) return (byte[]) value;
        if (value instanceof Byte) return new byte[]{(Byte) value};
        if (value instanceof Short) return ByteBuffer.allocate(2).order(ByteOrder.BIG_ENDIAN).putShort((Short) value).array();
        if (value instanceof Integer) return ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN).putInt((Integer) value).array();
        if (value instanceof Long) return ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN).putLong((Long) value).array();
        if (value instanceof Float) return ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN).putFloat((Float) value).array();
        if (value instanceof Double) return ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN).putDouble((Double) value).array();
        return String.valueOf(value).getBytes(StandardCharsets.UTF_8);
    }

    @NotNull
    private static byte[] toBytesWithWidth(long value, int byteWidth) {
        byte[] bytes = new byte[byteWidth];
        for (int i = 0; i < byteWidth; i++) {
            bytes[byteWidth - 1 - i] = (byte) ((value >> (i * 8)) & 0xFF);
        }
        return bytes;
    }

    // ------------------------------------------------------------------------
    // URL building helpers.
    // ------------------------------------------------------------------------

    @NotNull
    public static String urlEncode(@NotNull String value) {
        return URLEncoder.encode(value, StandardCharsets.UTF_8);
    }

    /**
     * RFC 3986 path-component encoder.
     *
     * <p>Crucially differs from {@link #urlEncode} ({@code application/x-www-form-urlencoded}):
     * the path encoder preserves the sub-delims ({@code !$&'()*+,;=}), the unreserved
     * pchar bytes ({@code -._~}), and {@code :} and {@code @} — all of which are valid
     * in a path segment per RFC 3986 §3.3. This matches the C++ httpcl
     * {@code URIComponents::appendPath} behaviour and is required for the OpenAPI path
     * styles {@code matrix} (uses {@code ;}, {@code =}) and {@code label} (uses {@code .}).
     *
     * <p>Without this distinction, a matrix-styled value like {@code ;id=42} would be
     * mangled to {@code %3Bid%3D42}, and the server would not recognise it as matrix syntax.
     */
    @NotNull
    public static String pathEncode(@NotNull String value) {
        byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
        StringBuilder out = new StringBuilder(bytes.length);
        for (byte raw : bytes) {
            int b = raw & 0xff;
            if (isUnreservedPChar(b)) {
                out.append((char) b);
            } else {
                out.append('%');
                out.append(HEX_DIGITS[(b >> 4) & 0xF]);
                out.append(HEX_DIGITS[b & 0xF]);
            }
        }
        return out.toString();
    }

    private static final char[] HEX_DIGITS = "0123456789ABCDEF".toCharArray();

    /**
     * True for bytes that are valid as-is in a path segment per RFC 3986 §3.3 (pchar):
     * unreserved | sub-delims | ":" | "@". '/' is NOT included — segment separator must
     * stay an escape candidate; callers handle the separator themselves.
     */
    private static boolean isUnreservedPChar(int b) {
        // unreserved: ALPHA / DIGIT / "-" / "." / "_" / "~"
        if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || (b >= '0' && b <= '9')) return true;
        switch (b) {
            // unreserved
            case '-': case '.': case '_': case '~':
            // sub-delims
            case '!': case '$': case '&': case '\'': case '(': case ')':
            case '*': case '+': case ',': case ';': case '=':
            // pchar additions
            case ':': case '@':
                return true;
            default:
                return false;
        }
    }

    /**
     * Builds a query string from an ordered list of {@code (name, value)} pairs,
     * preserving order and duplicates so that {@code style: form, explode: true}
     * yields the expected {@code ?id=1&id=2&id=3}.
     */
    @NotNull
    public static String buildQueryString(@NotNull List<Map.Entry<String, String>> pairs) {
        if (pairs.isEmpty()) return "";
        StringJoiner sj = new StringJoiner("&");
        for (Map.Entry<String, String> entry : pairs) {
            sj.add(urlEncode(entry.getKey()) + "=" + urlEncode(entry.getValue()));
        }
        return sj.toString();
    }
}
