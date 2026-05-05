package com.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * One alternative inside an OpenAPI {@code security:} list. The keys are
 * security-scheme names that ALL must be satisfied (AND); the outer list of
 * alternatives expresses the OR.
 *
 * <p>Mirrors C++ {@code SecurityRequirement} (a single alternative); see
 * {@code SecurityAlternatives} which is a {@code List<SecurityRequirement>}.
 */
public final class SecurityRequirement {
    private final Map<String, List<String>> required;

    public SecurityRequirement(@NotNull Map<String, List<String>> required) {
        Map<String, List<String>> copy = new LinkedHashMap<>();
        for (Map.Entry<String, List<String>> e : required.entrySet()) {
            copy.put(e.getKey(), Collections.unmodifiableList(new java.util.ArrayList<>(e.getValue())));
        }
        this.required = Collections.unmodifiableMap(copy);
    }

    /**
     * Map from security-scheme name to required OAuth2 scopes (empty list for
     * non-OAuth2 schemes). All entries must be satisfied for this alternative
     * to be considered fulfilled.
     */
    @NotNull
    public Map<String, List<String>> getSchemes() {
        return required;
    }
}
