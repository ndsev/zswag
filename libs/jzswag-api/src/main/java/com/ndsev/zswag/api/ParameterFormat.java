package com.ndsev.zswag.api;

/**
 * Parameter value encoding format for zserio types.
 */
public enum ParameterFormat {
    /**
     * String representation (default)
     */
    STRING,

    /**
     * Hexadecimal encoding (0x prefix)
     */
    HEX,

    /**
     * Standard Base64 encoding (RFC 4648)
     */
    BASE64,

    /**
     * Base64 URL-safe encoding (RFC 4648 Section 5)
     */
    BASE64URL,

    /**
     * Raw binary data
     */
    BINARY
}
