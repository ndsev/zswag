package io.github.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

/**
 * OpenAPI 3.0 security scheme. For HTTP, holds the scheme name (basic/bearer);
 * for API key, holds {@code in} location and parameter name; for OAuth2,
 * holds the {@code clientCredentials} flow's tokenUrl, refreshUrl, and the
 * map of available scopes.
 *
 * <p>Only the {@code clientCredentials} OAuth2 flow is supported; the parser
 * rejects schemes that declare other flows.
 */
public class SecurityScheme {
    private final String name;
    private final SecuritySchemeType type;
    private final String scheme;
    private final ParameterLocation apiKeyLocation;
    private final String apiKeyName;
    private final String tokenUrl;
    private final String refreshUrl;
    private final Map<String, String> oauth2Scopes;

    private SecurityScheme(Builder builder) {
        this.name = builder.name;
        this.type = builder.type;
        this.scheme = builder.scheme;
        this.apiKeyLocation = builder.apiKeyLocation;
        this.apiKeyName = builder.apiKeyName;
        this.tokenUrl = builder.tokenUrl;
        this.refreshUrl = builder.refreshUrl;
        this.oauth2Scopes = Collections.unmodifiableMap(new LinkedHashMap<>(builder.oauth2Scopes));
    }

    @NotNull public String getName() { return name; }
    @NotNull public SecuritySchemeType getType() { return type; }
    @Nullable public String getScheme() { return scheme; }
    @Nullable public ParameterLocation getApiKeyLocation() { return apiKeyLocation; }
    @Nullable public String getApiKeyName() { return apiKeyName; }

    /** OAuth2 token endpoint URL declared in the spec, if {@link SecuritySchemeType#OAUTH2}. */
    @NotNull public Optional<String> getTokenUrl() { return Optional.ofNullable(emptyToNull(tokenUrl)); }

    /** OAuth2 refresh URL declared in the spec, if any. */
    @NotNull public Optional<String> getRefreshUrl() { return Optional.ofNullable(emptyToNull(refreshUrl)); }

    /** Scope name → human description, as declared in the OAuth2 {@code clientCredentials} flow. */
    @NotNull public Map<String, String> getOAuth2Scopes() { return oauth2Scopes; }

    private static String emptyToNull(String s) { return (s == null || s.isEmpty()) ? null : s; }

    @NotNull
    public static Builder builder(@NotNull String name, @NotNull SecuritySchemeType type) {
        return new Builder(name, type);
    }

    public static class Builder {
        private final String name;
        private final SecuritySchemeType type;
        private String scheme;
        private ParameterLocation apiKeyLocation;
        private String apiKeyName;
        private String tokenUrl;
        private String refreshUrl;
        private Map<String, String> oauth2Scopes = new LinkedHashMap<>();

        private Builder(String name, SecuritySchemeType type) {
            this.name = name;
            this.type = type;
        }

        @NotNull public Builder scheme(@NotNull String scheme) { this.scheme = scheme; return this; }
        @NotNull public Builder apiKeyLocation(@NotNull ParameterLocation location) { this.apiKeyLocation = location; return this; }
        @NotNull public Builder apiKeyName(@NotNull String name) { this.apiKeyName = name; return this; }
        @NotNull public Builder tokenUrl(@Nullable String tokenUrl) { this.tokenUrl = tokenUrl; return this; }
        @NotNull public Builder refreshUrl(@Nullable String refreshUrl) { this.refreshUrl = refreshUrl; return this; }
        @NotNull public Builder oauth2Scopes(@NotNull Map<String, String> scopes) {
            this.oauth2Scopes = new LinkedHashMap<>(scopes);
            return this;
        }
        @NotNull public Builder addOAuth2Scope(@NotNull String name, @NotNull String description) {
            this.oauth2Scopes.put(name, description);
            return this;
        }

        @NotNull public SecurityScheme build() { return new SecurityScheme(this); }
    }
}
