package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

/**
 * Represents an OpenAPI parameter definition.
 */
public class OpenAPIParameter {
    private final String name;
    private final ParameterLocation location;
    private final ParameterStyle style;
    private final ParameterFormat format;
    private final boolean required;
    private final boolean explode;

    private OpenAPIParameter(Builder builder) {
        this.name = builder.name;
        this.location = builder.location;
        this.style = builder.style;
        this.format = builder.format != null ? builder.format : ParameterFormat.STRING;
        this.required = builder.required;
        this.explode = builder.explode;
    }

    @NotNull
    public String getName() {
        return name;
    }

    @NotNull
    public ParameterLocation getLocation() {
        return location;
    }

    @NotNull
    public ParameterStyle getStyle() {
        return style;
    }

    @NotNull
    public ParameterFormat getFormat() {
        return format;
    }

    public boolean isRequired() {
        return required;
    }

    public boolean isExplode() {
        return explode;
    }

    @NotNull
    public static Builder builder(@NotNull String name, @NotNull ParameterLocation location) {
        return new Builder(name, location);
    }

    public static class Builder {
        private final String name;
        private final ParameterLocation location;
        private ParameterStyle style;
        private ParameterFormat format;
        private boolean required;
        private boolean explode;

        private Builder(String name, ParameterLocation location) {
            this.name = name;
            this.location = location;
            // Set default style based on location
            this.style = getDefaultStyle(location);
            this.explode = false;
        }

        private static ParameterStyle getDefaultStyle(ParameterLocation location) {
            switch (location) {
                case PATH:
                case HEADER:
                    return ParameterStyle.SIMPLE;
                case QUERY:
                case COOKIE:
                    return ParameterStyle.FORM;
                default:
                    return ParameterStyle.SIMPLE;
            }
        }

        @NotNull
        public Builder style(@NotNull ParameterStyle style) {
            this.style = style;
            return this;
        }

        @NotNull
        public Builder format(@NotNull ParameterFormat format) {
            this.format = format;
            return this;
        }

        @NotNull
        public Builder required(boolean required) {
            this.required = required;
            return this;
        }

        @NotNull
        public Builder explode(boolean explode) {
            this.explode = explode;
            return this;
        }

        @NotNull
        public OpenAPIParameter build() {
            return new OpenAPIParameter(this);
        }
    }
}
