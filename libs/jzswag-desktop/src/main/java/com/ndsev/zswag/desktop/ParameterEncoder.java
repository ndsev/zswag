package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.*;

/**
 * Utility class for encoding parameter values according to OpenAPI specifications.
 * Handles different parameter styles (simple, label, matrix, form, etc.) and formats.
 */
public class ParameterEncoder {

    /**
     * Encodes a parameter value according to its style and format.
     * For arrays/collections, each element is formatted individually before joining.
     *
     * @param param The parameter definition
     * @param value The value to encode
     * @return The encoded string value
     */
    @NotNull
    public static String encodeParameter(@NotNull OpenAPIParameter param, @NotNull Object value) {
        // Handle primitive arrays directly to preserve exact byte width
        List<String> arrayElements = formatArrayElements(value, param.getFormat());
        if (arrayElements != null) {
            return applyArrayStyle(param.getName(), arrayElements, param.getStyle(), param.isExplode());
        }

        // Scalar value - format then apply style
        String formattedValue = formatScalarValue(value, param.getFormat());
        return applyStyle(param.getName(), formattedValue, param.getStyle(), param.isExplode());
    }

    /**
     * Formats array elements directly from primitive arrays to preserve correct byte width.
     * Returns null if value is not an array/collection.
     *
     * Byte width mapping (for base64/base64url/hex formats):
     * - byte[], Byte: 1 byte (int8)
     * - short[]: 1 byte (zserio uint8 stored as short in Java)
     * - int[]: 4 bytes (int32)
     * - long[]: 8 bytes (int64)
     * - float[]: 4 bytes (IEEE 754)
     * - double[]: 8 bytes (IEEE 754)
     */
    @Nullable
    private static List<String> formatArrayElements(@NotNull Object value, @NotNull ParameterFormat format) {
        if (value instanceof Collection) {
            // Collections - box each element
            List<String> result = new ArrayList<>();
            for (Object element : (Collection<?>) value) {
                result.add(formatScalarValue(element, format));
            }
            return result;
        } else if (value instanceof Object[]) {
            List<String> result = new ArrayList<>();
            for (Object element : (Object[]) value) {
                result.add(formatScalarValue(element, format));
            }
            return result;
        } else if (value instanceof short[]) {
            // short[] in zserio represents uint8 - encode each as 1 byte
            short[] arr = (short[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (short v : arr) {
                result.add(formatWithByteWidth(v, 1, format));
            }
            return result;
        } else if (value instanceof int[]) {
            // int[] represents int32 - encode each as 4 bytes
            int[] arr = (int[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (int v : arr) {
                result.add(formatWithByteWidth(v, 4, format));
            }
            return result;
        } else if (value instanceof long[]) {
            // long[] represents int64 - encode each as 8 bytes
            long[] arr = (long[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (long v : arr) {
                result.add(formatWithByteWidth(v, 8, format));
            }
            return result;
        } else if (value instanceof double[]) {
            double[] arr = (double[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (double v : arr) {
                result.add(formatScalarValue(v, format));
            }
            return result;
        } else if (value instanceof float[]) {
            float[] arr = (float[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (float v : arr) {
                result.add(formatScalarValue(v, format));
            }
            return result;
        } else if (value instanceof boolean[]) {
            boolean[] arr = (boolean[]) value;
            List<String> result = new ArrayList<>(arr.length);
            for (boolean v : arr) {
                result.add(formatScalarValue(v, format));
            }
            return result;
        } else if (value instanceof byte[]) {
            // byte[] is treated as binary data, not array of elements
            return null;
        }
        return null;
    }

    /**
     * Formats an integer value with a specific byte width for base64/hex encoding.
     */
    @NotNull
    private static String formatWithByteWidth(long value, int byteWidth, @NotNull ParameterFormat format) {
        switch (format) {
            case STRING:
                return String.valueOf(value);
            case HEX:
                return toSignedHexString(value);
            case BASE64:
                return toBase64WithWidth(value, byteWidth);
            case BASE64URL:
                return toBase64UrlWithWidth(value, byteWidth);
            case BINARY:
                return String.valueOf(value);
            default:
                return String.valueOf(value);
        }
    }

    /**
     * Converts a signed integer to hex string without "0x" prefix.
     * For signed values: uses sign prefix ("-") followed by hex of absolute value.
     * E.g., -200 → "-c8", 100 → "64"
     */
    @NotNull
    private static String toSignedHexString(long value) {
        if (value < 0) {
            return "-" + Long.toHexString(-value);
        }
        return Long.toHexString(value);
    }

    /**
     * Converts an integer to base64 with specific byte width (big-endian).
     */
    @NotNull
    private static String toBase64WithWidth(long value, int byteWidth) {
        byte[] bytes = toBytesWithWidth(value, byteWidth);
        return Base64.getEncoder().encodeToString(bytes);
    }

    /**
     * Converts an integer to base64url with specific byte width (big-endian).
     */
    @NotNull
    private static String toBase64UrlWithWidth(long value, int byteWidth) {
        byte[] bytes = toBytesWithWidth(value, byteWidth);
        return Base64.getUrlEncoder().encodeToString(bytes);
    }

    /**
     * Converts an integer to a byte array with specific width (big-endian).
     */
    @NotNull
    private static byte[] toBytesWithWidth(long value, int byteWidth) {
        byte[] bytes = new byte[byteWidth];
        for (int i = 0; i < byteWidth; i++) {
            bytes[byteWidth - 1 - i] = (byte) ((value >> (i * 8)) & 0xFF);
        }
        return bytes;
    }

    /**
     * Extracts elements from an array or collection.
     * Returns null if value is not an array/collection.
     */
    @SuppressWarnings("unchecked")
    private static List<Object> extractArrayElements(Object value) {
        if (value instanceof Collection) {
            return new ArrayList<>((Collection<?>) value);
        } else if (value instanceof Object[]) {
            return Arrays.asList((Object[]) value);
        } else if (value instanceof int[]) {
            int[] arr = (int[]) value;
            List<Object> list = new ArrayList<>(arr.length);
            for (int v : arr) list.add(v);
            return list;
        } else if (value instanceof short[]) {
            short[] arr = (short[]) value;
            List<Object> list = new ArrayList<>(arr.length);
            for (short v : arr) list.add(v);
            return list;
        } else if (value instanceof long[]) {
            long[] arr = (long[]) value;
            List<Object> list = new ArrayList<>(arr.length);
            for (long v : arr) list.add(v);
            return list;
        } else if (value instanceof double[]) {
            double[] arr = (double[]) value;
            List<Object> list = new ArrayList<>(arr.length);
            for (double v : arr) list.add(v);
            return list;
        } else if (value instanceof float[]) {
            float[] arr = (float[]) value;
            List<Object> list = new ArrayList<>(arr.length);
            for (float v : arr) list.add(v);
            return list;
        } else if (value instanceof boolean[]) {
            boolean[] arr = (boolean[]) value;
            List<Object> list = new ArrayList<>(arr.length);
            for (boolean v : arr) list.add(v);
            return list;
        } else if (value instanceof byte[]) {
            // byte[] is special - treated as binary data, not as array of elements
            return null;
        }
        return null;
    }

    /**
     * Applies style encoding for array values.
     */
    @NotNull
    private static String applyArrayStyle(@NotNull String name, @NotNull List<String> values,
                                          @NotNull ParameterStyle style, boolean explode) {
        if (values.isEmpty()) {
            return "";
        }

        switch (style) {
            case SIMPLE:
                return String.join(",", values);
            case LABEL:
                if (explode) {
                    return "." + String.join(".", values);
                }
                return "." + String.join(",", values);
            case MATRIX:
                if (explode) {
                    StringJoiner joiner = new StringJoiner("");
                    for (String v : values) {
                        joiner.add(";" + name + "=" + v);
                    }
                    return joiner.toString();
                }
                return ";" + name + "=" + String.join(",", values);
            case FORM:
                // For form style, values are comma-separated (explode handled at query level)
                return String.join(",", values);
            case SPACE_DELIMITED:
                return String.join(" ", values);
            case PIPE_DELIMITED:
                return String.join("|", values);
            case DEEP_OBJECT:
                // Deep object doesn't apply to arrays in the same way
                return String.join(",", values);
            default:
                return String.join(",", values);
        }
    }

    /**
     * Formats a scalar value according to the specified format.
     * For arrays, call this method on each element individually.
     */
    @NotNull
    private static String formatScalarValue(@NotNull Object value, @NotNull ParameterFormat format) {
        // Handle booleans specially - server expects "0" or "1", not "true" or "false"
        if (value instanceof Boolean) {
            return ((Boolean) value) ? "1" : "0";
        }

        switch (format) {
            case STRING:
                return String.valueOf(value);
            case HEX:
                return toHexString(value);
            case BASE64:
                return toBase64(value);
            case BASE64URL:
                return toBase64Url(value);
            case BINARY:
                // Binary format returns raw bytes - caller must handle appropriately
                return String.valueOf(value);
            default:
                return String.valueOf(value);
        }
    }

    /**
     * Applies parameter style encoding.
     */
    @NotNull
    private static String applyStyle(@NotNull String name, @NotNull String value,
                                      @NotNull ParameterStyle style, boolean explode) {
        switch (style) {
            case SIMPLE:
                return value;
            case LABEL:
                return "." + value;
            case MATRIX:
                return ";" + name + "=" + value;
            case FORM:
                return value;
            case SPACE_DELIMITED:
                return value.replace(",", " ");
            case PIPE_DELIMITED:
                return value.replace(",", "|");
            case DEEP_OBJECT:
                // Deep object requires special handling for nested structures
                return value;
            default:
                return value;
        }
    }

    /**
     * Converts value to hexadecimal string without prefix.
     * For signed integers: uses sign prefix ("-") followed by hex of absolute value.
     * E.g., -200 → "-c8", 100 → "64"
     */
    @NotNull
    private static String toHexString(@NotNull Object value) {
        if (value instanceof byte[]) {
            byte[] bytes = (byte[]) value;
            StringBuilder hex = new StringBuilder();
            for (byte b : bytes) {
                hex.append(String.format("%02x", b & 0xFF));
            }
            return hex.toString();
        } else if (value instanceof Number) {
            Number num = (Number) value;
            long longValue = num.longValue();
            if (longValue < 0) {
                return "-" + Long.toHexString(-longValue);
            }
            return Long.toHexString(longValue);
        } else {
            return String.valueOf(value);
        }
    }

    /**
     * Converts value to standard Base64 encoding (RFC 4648).
     * For numeric types, encodes the raw byte representation (big-endian).
     */
    @NotNull
    private static String toBase64(@NotNull Object value) {
        byte[] bytes = toBytes(value);
        return Base64.getEncoder().encodeToString(bytes);
    }

    /**
     * Converts value to URL-safe Base64 encoding (RFC 4648 Section 5).
     * For numeric types, encodes the raw byte representation (big-endian).
     * Includes padding for compatibility with server-side decoding.
     */
    @NotNull
    private static String toBase64Url(@NotNull Object value) {
        byte[] bytes = toBytes(value);
        return Base64.getUrlEncoder().encodeToString(bytes);
    }

    /**
     * Converts a value to its raw byte representation.
     * Numeric types are converted to big-endian byte arrays.
     * Strings are converted to UTF-8 bytes.
     * byte[] is returned as-is.
     */
    @NotNull
    private static byte[] toBytes(@NotNull Object value) {
        if (value instanceof byte[]) {
            return (byte[]) value;
        } else if (value instanceof Byte) {
            // Single byte
            return new byte[] { (Byte) value };
        } else if (value instanceof Short) {
            // 2 bytes, big-endian
            ByteBuffer buffer = ByteBuffer.allocate(2).order(ByteOrder.BIG_ENDIAN);
            buffer.putShort((Short) value);
            return buffer.array();
        } else if (value instanceof Integer) {
            // 4 bytes, big-endian
            ByteBuffer buffer = ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN);
            buffer.putInt((Integer) value);
            return buffer.array();
        } else if (value instanceof Long) {
            // 8 bytes, big-endian
            ByteBuffer buffer = ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN);
            buffer.putLong((Long) value);
            return buffer.array();
        } else if (value instanceof Float) {
            // 4 bytes, IEEE 754, big-endian
            ByteBuffer buffer = ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN);
            buffer.putFloat((Float) value);
            return buffer.array();
        } else if (value instanceof Double) {
            // 8 bytes, IEEE 754, big-endian
            ByteBuffer buffer = ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN);
            buffer.putDouble((Double) value);
            return buffer.array();
        } else {
            // Default: convert to string and encode as UTF-8
            return String.valueOf(value).getBytes(StandardCharsets.UTF_8);
        }
    }

    /**
     * URL-encodes a string value.
     */
    @NotNull
    public static String urlEncode(@NotNull String value) {
        return URLEncoder.encode(value, StandardCharsets.UTF_8);
    }

    /**
     * Builds a query string from parameters.
     */
    @NotNull
    public static String buildQueryString(@NotNull Map<String, String> parameters) {
        if (parameters.isEmpty()) {
            return "";
        }

        StringJoiner joiner = new StringJoiner("&");
        for (Map.Entry<String, String> entry : parameters.entrySet()) {
            String encodedName = urlEncode(entry.getKey());
            String encodedValue = urlEncode(entry.getValue());
            joiner.add(encodedName + "=" + encodedValue);
        }
        return joiner.toString();
    }
}
