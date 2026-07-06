package io.github.ndsev.zswag.api;

/**
 * OpenAPI security scheme types.
 */
public enum SecuritySchemeType {
    /**
     * HTTP authentication schemes (Basic, Bearer, etc.)
     */
    HTTP,

    /**
     * API key in query, header, or cookie
     */
    API_KEY,

    /**
     * OAuth2 flows
     */
    OAUTH2,

    /**
     * OpenID Connect Discovery
     */
    OPEN_ID_CONNECT
}
