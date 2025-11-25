package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

/**
 * Represents an OpenAPI security scheme.
 */
public class SecurityScheme {
    private final String name;
    private final SecuritySchemeType type;
    private final String scheme; // For HTTP type (e.g., "basic", "bearer")
    private final ParameterLocation apiKeyLocation; // For API key type
    private final String apiKeyName; // For API key type

    private SecurityScheme(Builder builder) {
        this.name = builder.name;
        this.type = builder.type;
        this.scheme = builder.scheme;
        this.apiKeyLocation = builder.apiKeyLocation;
        this.apiKeyName = builder.apiKeyName;
    }

    @NotNull
    public String getName() {
        return name;
    }

    @NotNull
    public SecuritySchemeType getType() {
        return type;
    }

    @Nullable
    public String getScheme() {
        return scheme;
    }

    @Nullable
    public ParameterLocation getApiKeyLocation() {
        return apiKeyLocation;
    }

    @Nullable
    public String getApiKeyName() {
        return apiKeyName;
    }

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

        private Builder(String name, SecuritySchemeType type) {
            this.name = name;
            this.type = type;
        }

        @NotNull
        public Builder scheme(@NotNull String scheme) {
            this.scheme = scheme;
            return this;
        }

        @NotNull
        public Builder apiKeyLocation(@NotNull ParameterLocation location) {
            this.apiKeyLocation = location;
            return this;
        }

        @NotNull
        public Builder apiKeyName(@NotNull String name) {
            this.apiKeyName = name;
            return this;
        }

        @NotNull
        public SecurityScheme build() {
            return new SecurityScheme(this);
        }
    }
}
