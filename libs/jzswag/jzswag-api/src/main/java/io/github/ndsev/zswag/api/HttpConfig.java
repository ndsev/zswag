package io.github.ndsev.zswag.api;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.time.Duration;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.regex.Pattern;

/**
 * Per-request HTTP configuration. Mirrors C++ {@code httpcl::Config} and Python
 * {@code zswag.HTTPConfig}: extra headers, query parameters, cookies, optional
 * basic-auth, proxy, OAuth2, and API key.
 *
 * <p>Instances are immutable. Use {@link Builder} to construct, {@link #toBuilder()}
 * to derive a modified copy, and {@link #mergedWith(HttpConfig)} to combine two
 * configs (the {@code other} config wins on scalar fields; multi-valued fields are
 * unioned).
 *
 * <p>When held inside an {@link HttpSettings} multi-scope registry, the optional
 * {@code scope} / {@code urlPattern} fields select which request URLs the config
 * applies to.
 */
public final class HttpConfig {
    private final Map<String, List<String>> headers;
    private final Map<String, List<String>> query;
    private final Map<String, String> cookies;
    @Nullable private final Duration timeout;
    @Nullable private final Boolean sslStrict;
    private final BasicAuthentication auth;
    private final Proxy proxy;
    private final OAuth2 oauth2;
    private final String apiKey;
    private final String scope;
    private final Pattern urlPattern;

    private HttpConfig(Builder builder) {
        this.headers = unmodifiableDeepCopy(builder.headers);
        this.query = unmodifiableDeepCopy(builder.query);
        this.cookies = Collections.unmodifiableMap(new LinkedHashMap<>(builder.cookies));
        this.timeout = builder.timeout;
        this.sslStrict = builder.sslStrict;
        this.auth = builder.auth;
        this.proxy = builder.proxy;
        this.oauth2 = builder.oauth2;
        this.apiKey = builder.apiKey;
        this.scope = builder.scope;
        this.urlPattern = builder.urlPattern;
    }

    private static Map<String, List<String>> unmodifiableDeepCopy(Map<String, List<String>> source) {
        Map<String, List<String>> copy = new LinkedHashMap<>();
        for (Map.Entry<String, List<String>> entry : source.entrySet()) {
            copy.put(entry.getKey(), Collections.unmodifiableList(new ArrayList<>(entry.getValue())));
        }
        return Collections.unmodifiableMap(copy);
    }

    @NotNull public Map<String, List<String>> getHeaders() { return headers; }
    @NotNull public Map<String, List<String>> getQuery() { return query; }
    @NotNull public Map<String, String> getCookies() { return cookies; }
    @NotNull public Duration getTimeout() { return timeout != null ? timeout : defaultTimeout(); }

    /**
     * Returns the raw timeout field — {@code null} means "no opinion" (the effective
     * value is determined by the transport's env-derived default). Used by transports
     * (e.g. {@code JvmHttpClient}) to distinguish "caller explicitly set 60s" from
     * "caller didn't touch it" so {@code HTTP_TIMEOUT} can override the latter.
     */
    @Nullable public Duration getTimeoutOrNull() { return timeout; }
    /**
     * Per-request SSL strictness override. Defaults to {@code true} meaning
     * "no opinion" — the effective SSL behavior is determined by the
     * {@code HTTP_SSL_STRICT} environment variable (matching C++/Python). An
     * explicit {@code false} on this config forces permissive mode regardless
     * of env (no C++ equivalent — Java-only extension).
     */
    public boolean isSslStrict() { return sslStrict == null || sslStrict; }
    @NotNull public Optional<BasicAuthentication> getAuth() { return Optional.ofNullable(auth); }
    @NotNull public Optional<Proxy> getProxy() { return Optional.ofNullable(proxy); }
    @NotNull public Optional<OAuth2> getOAuth2() { return Optional.ofNullable(oauth2); }
    @NotNull public Optional<String> getApiKey() { return Optional.ofNullable(apiKey); }
    @NotNull public Optional<String> getScope() { return Optional.ofNullable(scope); }
    @NotNull public Optional<Pattern> getUrlPattern() { return Optional.ofNullable(urlPattern); }

    /**
     * Returns the first header value for the given name, or empty if absent.
     */
    @NotNull
    public Optional<String> getHeader(@NotNull String name) {
        List<String> values = headers.get(name);
        return (values == null || values.isEmpty()) ? Optional.empty() : Optional.of(values.get(0));
    }

    /**
     * Returns a new {@code HttpConfig} merged with {@code other}. Mirrors C++
     * {@code Config::operator|=}: cookies, headers, query are unioned (other's
     * entries appended). Auth, proxy, apiKey, oauth2 from {@code other} replace
     * this config's values when present (oauth2 sub-fields merge field-by-field).
     */
    @NotNull
    public HttpConfig mergedWith(@NotNull HttpConfig other) {
        Builder b = toBuilder();
        for (Map.Entry<String, List<String>> e : other.headers.entrySet()) {
            for (String value : e.getValue()) b.addHeader(e.getKey(), value);
        }
        for (Map.Entry<String, List<String>> e : other.query.entrySet()) {
            for (String value : e.getValue()) b.addQuery(e.getKey(), value);
        }
        for (Map.Entry<String, String> e : other.cookies.entrySet()) {
            b.cookie(e.getKey(), e.getValue());
        }
        if (other.auth != null) b.auth(other.auth);
        if (other.proxy != null) b.proxy(other.proxy);
        if (other.apiKey != null) b.apiKey(other.apiKey);
        if (other.oauth2 != null) {
            b.oauth2(other.oauth2.mergedOnto(this.oauth2));
        }
        if (other.timeout != null) b.timeout(other.timeout);
        if (other.sslStrict != null) b.sslStrict(other.sslStrict);
        return b.build();
    }

    static Duration defaultTimeout() {
        return Duration.ofSeconds(60);
    }

    @NotNull public Builder toBuilder() { return new Builder(this); }
    @NotNull public static Builder builder() { return new Builder(); }

    /** Empty config — useful as a starting point for merging. */
    @NotNull
    public static HttpConfig empty() {
        return builder().build();
    }

    /**
     * Returns a redacted summary of this config suitable for logging.
     * Passwords, secrets, API keys are masked.
     */
    @NotNull
    public String toSafeString() {
        StringBuilder sb = new StringBuilder();
        if (auth != null) {
            sb.append("  - Basic auth: user=").append(auth.user);
            if (!auth.password.isEmpty()) sb.append(", password=****");
            if (!auth.keychain.isEmpty()) sb.append(", keychain=").append(auth.keychain);
            sb.append("\n");
        }
        if (oauth2 != null) {
            sb.append("  - OAuth2: clientId=").append(oauth2.clientId);
            if (!oauth2.clientSecret.isEmpty()) sb.append(", clientSecret=****");
            if (!oauth2.clientSecretKeychain.isEmpty()) sb.append(", clientSecretKeychain=").append(oauth2.clientSecretKeychain);
            if (!oauth2.tokenUrlOverride.isEmpty()) sb.append(", tokenUrl=").append(oauth2.tokenUrlOverride);
            if (!oauth2.audience.isEmpty()) sb.append(", audience=").append(oauth2.audience);
            sb.append("\n");
        }
        if (proxy != null) {
            sb.append("  - Proxy: ").append(proxy.host).append(":").append(proxy.port);
            if (!proxy.user.isEmpty()) sb.append(", user=").append(proxy.user).append(", password=****");
            sb.append("\n");
        }
        if (apiKey != null) sb.append("  - API key: ****\n");
        if (!cookies.isEmpty()) sb.append("  - Cookies: ").append(cookies.keySet()).append("\n");
        if (!headers.isEmpty()) {
            sb.append("  - Headers: ");
            for (Map.Entry<String, List<String>> entry : headers.entrySet()) {
                String k = entry.getKey();
                String redacted = (k.equalsIgnoreCase("Authorization") || k.toLowerCase().contains("token") || k.toLowerCase().contains("secret"))
                        ? "****" : String.join(",", entry.getValue());
                sb.append(k).append("=").append(redacted).append(" ");
            }
            sb.append("\n");
        }
        if (!query.isEmpty()) sb.append("  - Query keys: ").append(query.keySet()).append("\n");
        return sb.toString();
    }

    public static final class BasicAuthentication {
        @NotNull public final String user;
        @NotNull public final String password;
        @NotNull public final String keychain;

        public BasicAuthentication(@NotNull String user, @NotNull String password, @NotNull String keychain) {
            this.user = Objects.requireNonNull(user);
            this.password = Objects.requireNonNull(password);
            this.keychain = Objects.requireNonNull(keychain);
        }

        public static BasicAuthentication ofPassword(String user, String password) {
            return new BasicAuthentication(user, password, "");
        }

        public static BasicAuthentication ofKeychain(String user, String keychainService) {
            return new BasicAuthentication(user, "", keychainService);
        }
    }

    public static final class Proxy {
        @NotNull public final String host;
        public final int port;
        @NotNull public final String user;
        @NotNull public final String password;
        @NotNull public final String keychain;

        public Proxy(@NotNull String host, int port, @NotNull String user, @NotNull String password, @NotNull String keychain) {
            this.host = Objects.requireNonNull(host);
            this.port = port;
            this.user = Objects.requireNonNull(user);
            this.password = Objects.requireNonNull(password);
            this.keychain = Objects.requireNonNull(keychain);
        }
    }

    /**
     * OAuth2 client-credentials flow configuration. Mirrors C++ {@code Config::OAuth2}.
     */
    public static final class OAuth2 {
        public enum TokenEndpointAuthMethod {
            /** RFC 6749 Section 2.3.1: HTTP Basic with client_id/client_secret in Authorization header. */
            RFC6749_CLIENT_SECRET_BASIC,
            /** RFC 5849: OAuth 1.0 HMAC-SHA256 signature on the token request. */
            RFC5849_OAUTH1_SIGNATURE
        }

        // Explicit-set flags for non-string fields, used by mergedOnto to know
        // whether `this` actually configured the field or just carries the default.
        static final int FLAG_USE_FOR_SPEC_FETCH = 1 << 0;
        static final int FLAG_TOKEN_ENDPOINT_AUTH_METHOD = 1 << 1;
        static final int FLAG_NONCE_LENGTH = 1 << 2;

        @NotNull public final String clientId;
        @NotNull public final String clientSecret;
        @NotNull public final String clientSecretKeychain;
        @NotNull public final String tokenUrlOverride;
        @NotNull public final String refreshUrlOverride;
        @NotNull public final String audience;
        @NotNull public final List<String> scopesOverride;
        public final boolean useForSpecFetch;
        @NotNull public final TokenEndpointAuthMethod tokenEndpointAuthMethod;
        public final int nonceLength;
        private final int explicitFlags;

        public OAuth2(
                @NotNull String clientId,
                @NotNull String clientSecret,
                @NotNull String clientSecretKeychain,
                @NotNull String tokenUrlOverride,
                @NotNull String refreshUrlOverride,
                @NotNull String audience,
                @NotNull List<String> scopesOverride,
                boolean useForSpecFetch,
                @NotNull TokenEndpointAuthMethod tokenEndpointAuthMethod,
                int nonceLength) {
            // Public constructor: caller passed concrete values for everything,
            // so all non-string fields are treated as explicitly set.
            this(clientId, clientSecret, clientSecretKeychain, tokenUrlOverride,
                    refreshUrlOverride, audience, scopesOverride,
                    useForSpecFetch, tokenEndpointAuthMethod, nonceLength,
                    FLAG_USE_FOR_SPEC_FETCH | FLAG_TOKEN_ENDPOINT_AUTH_METHOD | FLAG_NONCE_LENGTH);
        }

        private OAuth2(
                @NotNull String clientId,
                @NotNull String clientSecret,
                @NotNull String clientSecretKeychain,
                @NotNull String tokenUrlOverride,
                @NotNull String refreshUrlOverride,
                @NotNull String audience,
                @NotNull List<String> scopesOverride,
                boolean useForSpecFetch,
                @NotNull TokenEndpointAuthMethod tokenEndpointAuthMethod,
                int nonceLength,
                int explicitFlags) {
            this.clientId = Objects.requireNonNull(clientId);
            this.clientSecret = Objects.requireNonNull(clientSecret);
            this.clientSecretKeychain = Objects.requireNonNull(clientSecretKeychain);
            this.tokenUrlOverride = Objects.requireNonNull(tokenUrlOverride);
            this.refreshUrlOverride = Objects.requireNonNull(refreshUrlOverride);
            this.audience = Objects.requireNonNull(audience);
            this.scopesOverride = Collections.unmodifiableList(new ArrayList<>(scopesOverride));
            this.useForSpecFetch = useForSpecFetch;
            this.tokenEndpointAuthMethod = Objects.requireNonNull(tokenEndpointAuthMethod);
            this.nonceLength = nonceLength;
            this.explicitFlags = explicitFlags;
        }

        @NotNull
        OAuth2 mergedOnto(@Nullable OAuth2 base) {
            if (base == null) return this;
            boolean newUseForSpecFetch = (explicitFlags & FLAG_USE_FOR_SPEC_FETCH) != 0
                    ? useForSpecFetch : base.useForSpecFetch;
            TokenEndpointAuthMethod newTokenAuthMethod = (explicitFlags & FLAG_TOKEN_ENDPOINT_AUTH_METHOD) != 0
                    ? tokenEndpointAuthMethod : base.tokenEndpointAuthMethod;
            int newNonceLength = (explicitFlags & FLAG_NONCE_LENGTH) != 0
                    ? nonceLength : base.nonceLength;
            // Union the flags so further merges still see the explicit-set state from either side.
            int mergedFlags = explicitFlags | base.explicitFlags;
            return new OAuth2(
                    !clientId.isEmpty() ? clientId : base.clientId,
                    !clientSecret.isEmpty() ? clientSecret : base.clientSecret,
                    !clientSecretKeychain.isEmpty() ? clientSecretKeychain : base.clientSecretKeychain,
                    !tokenUrlOverride.isEmpty() ? tokenUrlOverride : base.tokenUrlOverride,
                    !refreshUrlOverride.isEmpty() ? refreshUrlOverride : base.refreshUrlOverride,
                    !audience.isEmpty() ? audience : base.audience,
                    !scopesOverride.isEmpty() ? scopesOverride : base.scopesOverride,
                    newUseForSpecFetch,
                    newTokenAuthMethod,
                    newNonceLength,
                    mergedFlags);
        }

        public static Builder builder() { return new Builder(); }

        public static final class Builder {
            private String clientId = "";
            private String clientSecret = "";
            private String clientSecretKeychain = "";
            private String tokenUrlOverride = "";
            private String refreshUrlOverride = "";
            private String audience = "";
            private List<String> scopesOverride = new ArrayList<>();
            private boolean useForSpecFetch = true;
            private TokenEndpointAuthMethod tokenEndpointAuthMethod = TokenEndpointAuthMethod.RFC6749_CLIENT_SECRET_BASIC;
            private int nonceLength = 16;
            private int explicitFlags = 0;

            public Builder clientId(String v) { this.clientId = v == null ? "" : v; return this; }
            public Builder clientSecret(String v) { this.clientSecret = v == null ? "" : v; return this; }
            public Builder clientSecretKeychain(String v) { this.clientSecretKeychain = v == null ? "" : v; return this; }
            public Builder tokenUrl(String v) { this.tokenUrlOverride = v == null ? "" : v; return this; }
            public Builder refreshUrl(String v) { this.refreshUrlOverride = v == null ? "" : v; return this; }
            public Builder audience(String v) { this.audience = v == null ? "" : v; return this; }
            public Builder scopes(List<String> v) { this.scopesOverride = v == null ? new ArrayList<>() : new ArrayList<>(v); return this; }
            public Builder useForSpecFetch(boolean v) {
                this.useForSpecFetch = v;
                this.explicitFlags |= FLAG_USE_FOR_SPEC_FETCH;
                return this;
            }
            public Builder tokenEndpointAuthMethod(TokenEndpointAuthMethod v) {
                this.tokenEndpointAuthMethod = v;
                this.explicitFlags |= FLAG_TOKEN_ENDPOINT_AUTH_METHOD;
                return this;
            }
            public Builder nonceLength(int v) {
                if (v < 8 || v > 64) {
                    throw new IllegalArgumentException("tokenEndpointAuth.nonceLength must be between 8 and 64");
                }
                this.nonceLength = v;
                this.explicitFlags |= FLAG_NONCE_LENGTH;
                return this;
            }
            public OAuth2 build() {
                return new OAuth2(clientId, clientSecret, clientSecretKeychain, tokenUrlOverride,
                        refreshUrlOverride, audience, scopesOverride, useForSpecFetch,
                        tokenEndpointAuthMethod, nonceLength, explicitFlags);
            }
        }
    }

    public static final class Builder {
        private final Map<String, List<String>> headers = new LinkedHashMap<>();
        private final Map<String, List<String>> query = new LinkedHashMap<>();
        private final Map<String, String> cookies = new LinkedHashMap<>();
        @Nullable private Duration timeout;
        @Nullable private Boolean sslStrict;
        private BasicAuthentication auth;
        private Proxy proxy;
        private OAuth2 oauth2;
        private String apiKey;
        private String scope;
        private Pattern urlPattern;

        Builder() {}

        Builder(HttpConfig config) {
            for (Map.Entry<String, List<String>> e : config.headers.entrySet()) {
                this.headers.put(e.getKey(), new ArrayList<>(e.getValue()));
            }
            for (Map.Entry<String, List<String>> e : config.query.entrySet()) {
                this.query.put(e.getKey(), new ArrayList<>(e.getValue()));
            }
            this.cookies.putAll(config.cookies);
            this.timeout = config.timeout;
            this.sslStrict = config.sslStrict;
            this.auth = config.auth;
            this.proxy = config.proxy;
            this.oauth2 = config.oauth2;
            this.apiKey = config.apiKey;
            this.scope = config.scope;
            this.urlPattern = config.urlPattern;
        }

        @NotNull public Builder header(@NotNull String name, @NotNull String value) {
            this.headers.computeIfAbsent(name, k -> new ArrayList<>()).clear();
            this.headers.get(name).add(value);
            return this;
        }
        @NotNull public Builder addHeader(@NotNull String name, @NotNull String value) {
            this.headers.computeIfAbsent(name, k -> new ArrayList<>()).add(value);
            return this;
        }
        @NotNull public Builder headers(@NotNull Map<String, String> entries) {
            for (Map.Entry<String, String> e : entries.entrySet()) header(e.getKey(), e.getValue());
            return this;
        }

        @NotNull public Builder query(@NotNull String name, @NotNull String value) {
            this.query.computeIfAbsent(name, k -> new ArrayList<>()).clear();
            this.query.get(name).add(value);
            return this;
        }
        @NotNull public Builder addQuery(@NotNull String name, @NotNull String value) {
            this.query.computeIfAbsent(name, k -> new ArrayList<>()).add(value);
            return this;
        }

        @NotNull public Builder cookie(@NotNull String name, @NotNull String value) {
            this.cookies.put(name, value);
            return this;
        }
        @NotNull public Builder cookies(@NotNull Map<String, String> entries) {
            this.cookies.putAll(entries);
            return this;
        }

        @NotNull public Builder timeout(@NotNull Duration timeout) { this.timeout = timeout; return this; }
        @NotNull public Builder sslStrict(boolean sslStrict) { this.sslStrict = sslStrict; return this; }
        /** Clears the explicit-set state of timeout, restoring the inherited default behaviour. */
        @NotNull public Builder unsetTimeout() { this.timeout = null; return this; }
        /** Clears the explicit-set state of sslStrict, restoring "no opinion" — the
         *  effective behaviour is then determined by {@code HTTP_SSL_STRICT}. */
        @NotNull public Builder unsetSslStrict() { this.sslStrict = null; return this; }

        @NotNull public Builder auth(@Nullable BasicAuthentication auth) { this.auth = auth; return this; }
        @NotNull public Builder basicAuth(@NotNull String user, @NotNull String password) {
            this.auth = BasicAuthentication.ofPassword(user, password);
            return this;
        }

        @NotNull public Builder proxy(@Nullable Proxy proxy) { this.proxy = proxy; return this; }

        @NotNull public Builder oauth2(@Nullable OAuth2 oauth2) { this.oauth2 = oauth2; return this; }

        @NotNull public Builder apiKey(@Nullable String apiKey) { this.apiKey = apiKey; return this; }

        @NotNull public Builder bearerToken(@NotNull String token) {
            return header("Authorization", "Bearer " + token);
        }

        @NotNull public Builder scope(@Nullable String scope, @Nullable Pattern urlPattern) {
            this.scope = scope;
            this.urlPattern = urlPattern;
            return this;
        }

        @NotNull public HttpConfig build() { return new HttpConfig(this); }
    }
}
