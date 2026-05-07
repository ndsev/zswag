package io.github.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.regex.Pattern;

/**
 * Multi-scope HTTP settings registry. Mirrors C++ {@code httpcl::Settings}: an
 * ordered list of {@link HttpConfig} entries, each with an optional URL scope
 * (glob-like pattern compiled to regex). For a given request URL, all matching
 * entries are merged into a single effective {@link HttpConfig}.
 *
 * <p>Loading from {@code HTTP_SETTINGS_FILE} is performed by
 * {@code HttpSettingsLoader} in jzswag-jvm (which keeps this module free of
 * a YAML dependency).
 */
public final class HttpSettings {
    private final List<HttpConfig> entries;

    public HttpSettings(@NotNull List<HttpConfig> entries) {
        this.entries = Collections.unmodifiableList(new ArrayList<>(entries));
    }

    /** Empty settings — useful as a default when {@code HTTP_SETTINGS_FILE} is unset. */
    @NotNull
    public static HttpSettings empty() {
        return new HttpSettings(Collections.emptyList());
    }

    @NotNull
    public List<HttpConfig> getEntries() {
        return entries;
    }

    /**
     * Returns the merged {@link HttpConfig} for all entries whose
     * {@code urlPattern} matches the given URL. Iterates in declaration order;
     * each match is merged onto the accumulated result via
     * {@link HttpConfig#mergedWith(HttpConfig)}.
     *
     * <p>Mirrors C++ {@code Settings::operator[](url)}.
     */
    @NotNull
    public HttpConfig forUrl(@NotNull String url) {
        HttpConfig result = HttpConfig.empty();
        for (HttpConfig entry : entries) {
            Optional<Pattern> pattern = entry.getUrlPattern();
            if (!pattern.isPresent() || pattern.get().matcher(url).matches()) {
                result = result.mergedWith(entry);
            }
        }
        return result;
    }

    /**
     * Converts a glob-like scope pattern (with {@code *} as wildcard) into a
     * compiled regex, escaping all other regex metacharacters. Mirrors C++
     * {@code convertToRegex} in {@code http-settings.cpp}.
     */
    @NotNull
    public static Pattern compileScope(@NotNull String scope) {
        StringBuilder sb = new StringBuilder("^");
        for (int i = 0; i < scope.length(); i++) {
            char c = scope.charAt(i);
            switch (c) {
                case '*':
                    sb.append(".*");
                    break;
                case '.':
                    sb.append("\\.");
                    break;
                case '\\':
                    sb.append("\\\\");
                    break;
                case '^': case '$': case '|': case '(': case ')':
                case '[': case ']': case '{': case '}': case '?':
                case '+': case '-': case '!':
                    sb.append('\\').append(c);
                    break;
                default:
                    sb.append(c);
            }
        }
        sb.append(".*$");
        return Pattern.compile(sb.toString());
    }
}
