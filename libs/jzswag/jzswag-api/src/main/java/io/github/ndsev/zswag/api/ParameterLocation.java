package io.github.ndsev.zswag.api;

/**
 * Specifies where a parameter appears in the HTTP request.
 * Corresponds to OpenAPI parameter 'in' field.
 */
public enum ParameterLocation {
    /**
     * Parameter is part of the URL path (e.g., /users/{id})
     */
    PATH,

    /**
     * Parameter is in the query string (e.g., ?page=1&amp;limit=10)
     */
    QUERY,

    /**
     * Parameter is in HTTP headers
     */
    HEADER,

    /**
     * Parameter is in cookies
     */
    COOKIE
}
