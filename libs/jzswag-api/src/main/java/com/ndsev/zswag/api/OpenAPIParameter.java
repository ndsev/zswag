package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.util.Optional;

/**
 * One OpenAPI operation parameter, enriched with the zswag-specific
 * {@code x-zserio-request-part} extension that maps the parameter to a
 * field path in the zserio request object.
 */
public class OpenAPIParameter {
    /** Sentinel: when {@code requestPart == "*"}, the whole serialized request object goes here. */
    public static final String REQUEST_PART_WHOLE = "*";

    private final String name;
    private final ParameterLocation location;
    private final ParameterStyle style;
    private final ParameterFormat format;
    private final boolean required;
    private final boolean explode;
    private final String requestPart;  // null if no x-zserio-request-part on this parameter

    private OpenAPIParameter(Builder builder) {
        this.name = builder.name;
        this.location = builder.location;
        this.style = builder.style;
        this.format = builder.format != null ? builder.format : ParameterFormat.STRING;
        this.required = builder.required;
        this.explode = builder.explode;
        this.requestPart = builder.requestPart;
    }

    @NotNull public String getName() { return name; }
    @NotNull public ParameterLocation getLocation() { return location; }
    @NotNull public ParameterStyle getStyle() { return style; }
    @NotNull public ParameterFormat getFormat() { return format; }
    public boolean isRequired() { return required; }
    public boolean isExplode() { return explode; }

    /**
     * The {@code x-zserio-request-part} value: a dotted path into the zserio
     * request struct (e.g. {@code "base.value"}), or {@code "*"} for the whole
     * object as a binary blob, or empty if the parameter is not zswag-bound.
     */
    @NotNull
    public Optional<String> getRequestPart() {
        return Optional.ofNullable(requestPart);
    }

    public boolean isWholeRequest() {
        return REQUEST_PART_WHOLE.equals(requestPart);
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
        private String requestPart;

        private Builder(String name, ParameterLocation location) {
            this.name = name;
            this.location = location;
            this.style = getDefaultStyle(location);
            this.explode = (location == ParameterLocation.QUERY || location == ParameterLocation.COOKIE);
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

        @NotNull public Builder style(@NotNull ParameterStyle style) { this.style = style; return this; }
        @NotNull public Builder format(@NotNull ParameterFormat format) { this.format = format; return this; }
        @NotNull public Builder required(boolean required) { this.required = required; return this; }
        @NotNull public Builder explode(boolean explode) { this.explode = explode; return this; }
        @NotNull public Builder requestPart(@Nullable String requestPart) { this.requestPart = requestPart; return this; }

        @NotNull public OpenAPIParameter build() { return new OpenAPIParameter(this); }
    }
}
