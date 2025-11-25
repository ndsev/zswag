package com.ndsev.zswag.api;

/**
 * OpenAPI parameter serialization styles.
 * Defines how parameter values are serialized in HTTP requests.
 */
public enum ParameterStyle {
    /**
     * Simple style (default for path and header parameters)
     * Example: /users/5 or X-Header: 3,4,5
     */
    SIMPLE,

    /**
     * Label style (for path parameters)
     * Example: /users/.5
     */
    LABEL,

    /**
     * Matrix style (for path parameters)
     * Example: /users/;id=5
     */
    MATRIX,

    /**
     * Form style (default for query and cookie parameters)
     * Example: ?id=3&amp;id=4&amp;id=5
     */
    FORM,

    /**
     * Space-delimited arrays
     * Example: ?ids=3%204%205
     */
    SPACE_DELIMITED,

    /**
     * Pipe-delimited arrays
     * Example: ?ids=3|4|5
     */
    PIPE_DELIMITED,

    /**
     * Deep object style (for nested objects)
     * Example: ?color[R]=100&amp;color[G]=200
     */
    DEEP_OBJECT
}
