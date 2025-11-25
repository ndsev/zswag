package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;

import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.*;

/**
 * Utility class for encoding parameter values according to OpenAPI specifications.
 * Handles different parameter styles (simple, label, matrix, form, etc.) and formats.
 */
public class ParameterEncoder {

    /**
     * Encodes a parameter value according to its style and format.
     *
     * @param param The parameter definition
     * @param value The value to encode
     * @return The encoded string value
     */
    @NotNull
    public static String encodeParameter(@NotNull OpenAPIParameter param, @NotNull Object value) {
        // First, format the value if needed
        String formattedValue = formatValue(value, param.getFormat());

        // Then apply the parameter style
        return applyStyle(param.getName(), formattedValue, param.getStyle(), param.isExplode());
    }

    /**
     * Formats a value according to the specified format.
     */
    @NotNull
    private static String formatValue(@NotNull Object value, @NotNull ParameterFormat format) {
        switch (format) {
            case STRING:
                return valueToString(value);
            case HEX:
                return toHexString(value);
            case BASE64:
                return toBase64(value);
            case BASE64URL:
                return toBase64Url(value);
            case BINARY:
                // Binary format returns raw bytes - caller must handle appropriately
                return valueToString(value);
            default:
                return valueToString(value);
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
     * Converts a value to string representation.
     * Handles primitives, arrays, and collections.
     */
    @NotNull
    private static String valueToString(@NotNull Object value) {
        if (value instanceof Collection) {
            Collection<?> collection = (Collection<?>) value;
            StringJoiner joiner = new StringJoiner(",");
            for (Object item : collection) {
                joiner.add(String.valueOf(item));
            }
            return joiner.toString();
        } else if (value instanceof Object[]) {
            StringJoiner joiner = new StringJoiner(",");
            for (Object item : (Object[]) value) {
                joiner.add(String.valueOf(item));
            }
            return joiner.toString();
        } else if (value instanceof byte[]) {
            return Base64.getEncoder().encodeToString((byte[]) value);
        } else {
            return String.valueOf(value);
        }
    }

    /**
     * Converts value to hexadecimal string with 0x prefix.
     */
    @NotNull
    private static String toHexString(@NotNull Object value) {
        if (value instanceof byte[]) {
            byte[] bytes = (byte[]) value;
            StringBuilder hex = new StringBuilder("0x");
            for (byte b : bytes) {
                hex.append(String.format("%02x", b));
            }
            return hex.toString();
        } else if (value instanceof Number) {
            return "0x" + Long.toHexString(((Number) value).longValue());
        } else {
            return valueToString(value);
        }
    }

    /**
     * Converts value to standard Base64 encoding (RFC 4648).
     */
    @NotNull
    private static String toBase64(@NotNull Object value) {
        if (value instanceof byte[]) {
            return Base64.getEncoder().encodeToString((byte[]) value);
        } else {
            return Base64.getEncoder().encodeToString(valueToString(value).getBytes(StandardCharsets.UTF_8));
        }
    }

    /**
     * Converts value to URL-safe Base64 encoding (RFC 4648 Section 5).
     */
    @NotNull
    private static String toBase64Url(@NotNull Object value) {
        if (value instanceof byte[]) {
            return Base64.getUrlEncoder().withoutPadding().encodeToString((byte[]) value);
        } else {
            return Base64.getUrlEncoder().withoutPadding()
                    .encodeToString(valueToString(value).getBytes(StandardCharsets.UTF_8));
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
