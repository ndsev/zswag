package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpSettings;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.yaml.snakeyaml.LoaderOptions;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.constructor.SafeConstructor;

import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Loads {@link HttpSettings} from a YAML file matching the C++/Python schema
 * documented under "HTTP Settings File Format" in README.md.
 *
 * <p>Top-level shape:
 * <pre>{@code
 * http-settings:
 *   - scope: <glob>           # or url: <regex>
 *     basic-auth: { user, password|keychain }
 *     proxy: { host, port, user?, password|keychain? }
 *     cookies: { ... }
 *     headers: { ... }
 *     query: { ... }
 *     api-key: <value>
 *     oauth2:
 *       clientId, clientSecret|clientSecretKeychain,
 *       tokenUrl?, refreshUrl?, audience?, scope?, useForSpecFetch?,
 *       tokenEndpointAuth: { method, nonceLength? }
 * }</pre>
 *
 * <p>Legacy schema (top-level entries treated as a single un-scoped config) is
 * also accepted, matching C++ {@code http-settings.cpp:466-469}.
 */
public final class HttpSettingsLoader {
    private static final Logger logger = LoggerFactory.getLogger(HttpSettingsLoader.class);

    public static final String ENV_SETTINGS_FILE = "HTTP_SETTINGS_FILE";

    private HttpSettingsLoader() {}

    /**
     * Tracks the source path that {@link #loadFromEnvironment} most recently resolved,
     * so {@link HotReloader} can rebuild a fresh {@link HttpSettings} when the file
     * changes on disk. Per-thread? No — the env var is process-wide and reading it
     * twice in close succession is fine. Lazy holder keeps things thread-safe.
     */
    @org.jetbrains.annotations.Nullable
    public static Path environmentSourcePath() {
        String path = System.getenv(ENV_SETTINGS_FILE);
        if (path == null || path.isEmpty()) return null;
        Path file = Paths.get(path);
        return Files.isRegularFile(file) ? file : null;
    }

    /**
     * Tracks an {@link HttpSettings} object that gets re-read from disk when the
     * source file's last-modified timestamp advances. Mirrors C++
     * {@code httpcl::Settings::operator[]} (http-settings.cpp:520-543) which checks
     * mtime per call and re-parses on change — supports credential rotation in
     * long-running clients.
     *
     * <p>Thread-safe via double-checked locking on the {@code current} reference.
     * Failed reloads log a warning and keep the previous snapshot rather than
     * dropping to empty (better than losing all credentials mid-flight).
     */
    public static final class HotReloader {
        @org.jetbrains.annotations.Nullable
        private final Path source;
        private final java.util.concurrent.atomic.AtomicReference<HttpSettings> current;
        private volatile long lastMtimeMillis;

        private HotReloader(@org.jetbrains.annotations.Nullable Path source, @NotNull HttpSettings initial) {
            this.source = source;
            this.current = new java.util.concurrent.atomic.AtomicReference<>(initial);
            this.lastMtimeMillis = readMtimeOrZero();
        }

        /** Builds a reloader wired to {@code HTTP_SETTINGS_FILE} (or a no-op one if unset). */
        @NotNull
        public static HotReloader fromEnvironment() {
            Path src = environmentSourcePath();
            return new HotReloader(src, loadFromEnvironment());
        }

        /** Builds a reloader against an explicit path (or a no-op one if {@code source} null). */
        @NotNull
        public static HotReloader of(@org.jetbrains.annotations.Nullable Path source, @NotNull HttpSettings initial) {
            return new HotReloader(source, initial);
        }

        /**
         * Returns the current settings, reloading from disk if the source file's mtime
         * has advanced since last call. Calling this once per request is cheap (single
         * {@code stat}), comparable to the C++ implementation.
         */
        @NotNull
        public HttpSettings current() {
            if (source == null) return current.get();
            long mtime = readMtimeOrZero();
            if (mtime > lastMtimeMillis) {
                synchronized (this) {
                    if (mtime > lastMtimeMillis) {
                        try {
                            HttpSettings reloaded = loadFromFile(source);
                            current.set(reloaded);
                            lastMtimeMillis = mtime;
                            logger.debug("Reloaded HTTP_SETTINGS_FILE from '{}' (mtime advanced).", source);
                        } catch (IOException | RuntimeException e) {
                            // SnakeYAML throws ParserException (RuntimeException) on malformed YAML;
                            // IOException on disk failures. Either way: keep the old snapshot
                            // rather than dropping to empty during an in-flight rotation.
                            logger.warn("Failed to reload HTTP_SETTINGS_FILE '{}': {}. "
                                    + "Keeping previous snapshot.", source, e.getMessage());
                            // Bump lastMtimeMillis so we don't try to reload the same broken
                            // file every request.
                            lastMtimeMillis = mtime;
                        }
                    }
                }
            }
            return current.get();
        }

        private long readMtimeOrZero() {
            if (source == null) return 0L;
            try {
                return Files.getLastModifiedTime(source).toMillis();
            } catch (IOException e) {
                return 0L;
            }
        }
    }

    /**
     * Loads settings from {@code HTTP_SETTINGS_FILE} if set; returns empty
     * settings otherwise. Empty/unset env var, or non-existent path, yield
     * empty settings (logged at debug level), matching C++ semantics.
     */
    @NotNull
    public static HttpSettings loadFromEnvironment() {
        String path = System.getenv(ENV_SETTINGS_FILE);
        if (path == null || path.isEmpty()) {
            logger.debug("HTTP_SETTINGS_FILE environment variable is empty.");
            return HttpSettings.empty();
        }
        Path file = Paths.get(path);
        if (!Files.isRegularFile(file)) {
            logger.debug("The HTTP_SETTINGS_FILE path '{}' is not a file.", path);
            return HttpSettings.empty();
        }
        try {
            return loadFromFile(file);
        } catch (IOException e) {
            logger.error("Failed to read http-settings from '{}': {}", path, e.getMessage());
            return HttpSettings.empty();
        }
    }

    @NotNull
    public static HttpSettings loadFromFile(@NotNull Path file) throws IOException {
        try (InputStream input = Files.newInputStream(file)) {
            LoaderOptions options = new LoaderOptions();
            options.setAllowDuplicateKeys(false);
            Yaml yaml = new Yaml(new SafeConstructor(options));
            Object root = yaml.load(input);
            return parseRoot(root);
        }
    }

    @NotNull
    @SuppressWarnings("unchecked")
    static HttpSettings parseRoot(@Nullable Object root) {
        if (root == null) {
            return HttpSettings.empty();
        }
        List<Map<String, Object>> entries;
        if (root instanceof Map) {
            Map<String, Object> map = (Map<String, Object>) root;
            Object node = map.get("http-settings");
            if (node == null) {
                logger.debug("No 'http-settings' section found in YAML.");
                return HttpSettings.empty();
            }
            if (!(node instanceof List)) {
                throw new IllegalArgumentException("'http-settings' must be a list");
            }
            entries = (List<Map<String, Object>>) node;
        } else if (root instanceof List) {
            entries = (List<Map<String, Object>>) root;
        } else {
            throw new IllegalArgumentException(
                    "Top-level YAML must be a map with 'http-settings' key, or a list");
        }

        List<HttpConfig> configs = new ArrayList<>();
        for (Map<String, Object> entry : entries) {
            configs.add(parseEntry(entry));
        }
        return new HttpSettings(configs);
    }

    @SuppressWarnings("unchecked")
    private static HttpConfig parseEntry(@NotNull Map<String, Object> entry) {
        HttpConfig.Builder b = HttpConfig.builder();

        if (entry.containsKey("url")) {
            String url = String.valueOf(entry.get("url"));
            b.scope(null, java.util.regex.Pattern.compile(url));
        } else {
            String scope = entry.containsKey("scope") ? String.valueOf(entry.get("scope")) : "*";
            b.scope(scope, HttpSettings.compileScope(scope));
        }

        Object cookies = entry.get("cookies");
        if (cookies instanceof Map) {
            for (Map.Entry<?, ?> e : ((Map<?, ?>) cookies).entrySet()) {
                b.cookie(String.valueOf(e.getKey()), String.valueOf(e.getValue()));
            }
        }

        Object headers = entry.get("headers");
        if (headers instanceof Map) {
            for (Map.Entry<?, ?> e : ((Map<?, ?>) headers).entrySet()) {
                b.addHeader(String.valueOf(e.getKey()), String.valueOf(e.getValue()));
            }
        }

        Object query = entry.get("query");
        if (query instanceof Map) {
            for (Map.Entry<?, ?> e : ((Map<?, ?>) query).entrySet()) {
                b.addQuery(String.valueOf(e.getKey()), String.valueOf(e.getValue()));
            }
        }

        Object basicAuth = entry.get("basic-auth");
        if (basicAuth instanceof Map) {
            Map<String, Object> ba = (Map<String, Object>) basicAuth;
            String user = optString(ba, "user");
            if (user == null) {
                throw new IllegalArgumentException("basic-auth requires 'user'");
            }
            String password = optString(ba, "password");
            String keychain = optString(ba, "keychain");
            if (password == null && keychain == null) {
                throw new IllegalArgumentException("basic-auth requires either 'password' or 'keychain'");
            }
            b.auth(new HttpConfig.BasicAuthentication(
                    user,
                    password != null ? password : "",
                    keychain != null ? keychain : ""));
        }

        Object proxy = entry.get("proxy");
        if (proxy instanceof Map) {
            Map<String, Object> p = (Map<String, Object>) proxy;
            String host = optString(p, "host");
            Integer port = optInt(p, "port");
            if (host == null || port == null) {
                throw new IllegalArgumentException("proxy requires 'host' and 'port'");
            }
            String user = optString(p, "user");
            String password = optString(p, "password");
            String keychain = optString(p, "keychain");
            if (user != null && password == null && keychain == null) {
                throw new IllegalArgumentException("proxy with 'user' requires 'password' or 'keychain'");
            }
            b.proxy(new HttpConfig.Proxy(
                    host, port,
                    user != null ? user : "",
                    password != null ? password : "",
                    keychain != null ? keychain : ""));
        }

        Object apiKey = entry.get("api-key");
        if (apiKey instanceof String) {
            b.apiKey((String) apiKey);
        }

        Object oauth2 = entry.get("oauth2");
        if (oauth2 instanceof Map) {
            b.oauth2(parseOAuth2((Map<String, Object>) oauth2));
        }

        return b.build();
    }

    private static HttpConfig.OAuth2 parseOAuth2(@NotNull Map<String, Object> node) {
        HttpConfig.OAuth2.Builder b = HttpConfig.OAuth2.builder()
                .clientId(optString(node, "clientId"))
                .clientSecret(optString(node, "clientSecret"))
                .clientSecretKeychain(optString(node, "clientSecretKeychain"))
                .tokenUrl(optString(node, "tokenUrl"))
                .refreshUrl(optString(node, "refreshUrl"))
                .audience(optString(node, "audience"));

        Object scope = node.get("scope");
        if (scope instanceof List) {
            List<String> scopes = new ArrayList<>();
            for (Object s : (List<?>) scope) scopes.add(String.valueOf(s));
            b.scopes(scopes);
        }

        Object useForSpecFetch = node.get("useForSpecFetch");
        if (useForSpecFetch instanceof Boolean) {
            b.useForSpecFetch((Boolean) useForSpecFetch);
        }

        Object tea = node.get("tokenEndpointAuth");
        if (tea instanceof Map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> teaMap = (Map<String, Object>) tea;
            String method = optString(teaMap, "method");
            if (method != null) {
                switch (method) {
                    case "rfc6749-client-secret-basic":
                        b.tokenEndpointAuthMethod(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC6749_CLIENT_SECRET_BASIC);
                        break;
                    case "rfc5849-oauth1-signature":
                        b.tokenEndpointAuthMethod(HttpConfig.OAuth2.TokenEndpointAuthMethod.RFC5849_OAUTH1_SIGNATURE);
                        break;
                    default:
                        throw new IllegalArgumentException("Unknown tokenEndpointAuth method: " + method);
                }
            }
            Integer nonceLength = optInt(teaMap, "nonceLength");
            if (nonceLength != null) {
                b.nonceLength(nonceLength);
            }
        }

        return b.build();
    }

    // ------------------------------------------------------------------------
    // Write-back: serialize HttpSettings back to YAML.
    //
    // Mirrors C++ Settings::store (http-settings.cpp:484). Useful for tooling
    // that updates credentials programmatically and re-writes the settings file.
    // The HotReloader on the active HTTP client will pick the change up
    // automatically on the next request.
    // ------------------------------------------------------------------------

    /**
     * Writes a {@link HttpSettings} snapshot to a YAML file in the same schema this
     * loader reads. Secrets are written verbatim; the caller is responsible for
     * choosing whether to embed cleartext passwords or keychain references when
     * building the {@link HttpConfig} entries.
     *
     * @param destination path to write to (will be created or overwritten)
     * @param settings    snapshot to serialize
     * @throws IOException on filesystem failure
     */
    public static void writeToFile(@NotNull Path destination, @NotNull HttpSettings settings) throws IOException {
        try (BufferedWriter writer = Files.newBufferedWriter(destination)) {
            org.yaml.snakeyaml.DumperOptions opts = new org.yaml.snakeyaml.DumperOptions();
            opts.setDefaultFlowStyle(org.yaml.snakeyaml.DumperOptions.FlowStyle.BLOCK);
            opts.setIndent(2);
            opts.setPrettyFlow(true);
            new Yaml(opts).dump(settingsToYamlTree(settings), writer);
        }
    }

    /** Convert HttpSettings → POJO tree (Maps/Lists/Strings) for SnakeYAML's dump(). */
    @NotNull
    private static Map<String, Object> settingsToYamlTree(@NotNull HttpSettings settings) {
        List<Map<String, Object>> entries = new ArrayList<>();
        for (HttpConfig config : settings.getEntries()) {
            entries.add(configToYamlTree(config));
        }
        Map<String, Object> root = new LinkedHashMap<>();
        root.put("http-settings", entries);
        return root;
    }

    @NotNull
    private static Map<String, Object> configToYamlTree(@NotNull HttpConfig config) {
        Map<String, Object> e = new LinkedHashMap<>();
        // Scope: prefer the original scope glob, fall back to urlPattern if only that's set.
        config.getScope().ifPresent(s -> e.put("scope", s));
        if (!config.getScope().isPresent() && config.getUrlPattern().isPresent()) {
            e.put("url", config.getUrlPattern().get().pattern());
        }
        config.getAuth().ifPresent(auth -> {
            Map<String, Object> a = new LinkedHashMap<>();
            a.put("user", auth.user);
            if (!auth.password.isEmpty()) a.put("password", auth.password);
            if (!auth.keychain.isEmpty()) a.put("keychain", auth.keychain);
            e.put("basic-auth", a);
        });
        config.getProxy().ifPresent(p -> {
            Map<String, Object> proxy = new LinkedHashMap<>();
            proxy.put("host", p.host);
            proxy.put("port", p.port);
            if (!p.user.isEmpty()) proxy.put("user", p.user);
            if (!p.password.isEmpty()) proxy.put("password", p.password);
            if (!p.keychain.isEmpty()) proxy.put("keychain", p.keychain);
            e.put("proxy", proxy);
        });
        if (!config.getCookies().isEmpty()) e.put("cookies", new LinkedHashMap<>(config.getCookies()));
        if (!config.getHeaders().isEmpty()) {
            // Flatten single-value headers; preserve list form for multi-valued.
            Map<String, Object> headers = new LinkedHashMap<>();
            for (Map.Entry<String, List<String>> h : config.getHeaders().entrySet()) {
                headers.put(h.getKey(), h.getValue().size() == 1 ? h.getValue().get(0) : new ArrayList<>(h.getValue()));
            }
            e.put("headers", headers);
        }
        if (!config.getQuery().isEmpty()) {
            Map<String, Object> query = new LinkedHashMap<>();
            for (Map.Entry<String, List<String>> q : config.getQuery().entrySet()) {
                query.put(q.getKey(), q.getValue().size() == 1 ? q.getValue().get(0) : new ArrayList<>(q.getValue()));
            }
            e.put("query", query);
        }
        config.getApiKey().ifPresent(k -> e.put("api-key", k));
        config.getOAuth2().ifPresent(o -> {
            Map<String, Object> oauth = new LinkedHashMap<>();
            if (!o.clientId.isEmpty()) oauth.put("clientId", o.clientId);
            if (!o.clientSecret.isEmpty()) oauth.put("clientSecret", o.clientSecret);
            if (!o.clientSecretKeychain.isEmpty()) oauth.put("clientSecretKeychain", o.clientSecretKeychain);
            if (!o.tokenUrlOverride.isEmpty()) oauth.put("tokenUrl", o.tokenUrlOverride);
            if (!o.refreshUrlOverride.isEmpty()) oauth.put("refreshUrl", o.refreshUrlOverride);
            if (!o.audience.isEmpty()) oauth.put("audience", o.audience);
            if (!o.scopesOverride.isEmpty()) oauth.put("scope", new ArrayList<>(o.scopesOverride));
            e.put("oauth2", oauth);
        });
        return e;
    }

    @Nullable
    private static String optString(@NotNull Map<String, Object> map, @NotNull String key) {
        Object v = map.get(key);
        return v == null ? null : String.valueOf(v);
    }

    @Nullable
    private static Integer optInt(@NotNull Map<String, Object> map, @NotNull String key) {
        Object v = map.get(key);
        if (v == null) return null;
        if (v instanceof Number) return ((Number) v).intValue();
        try {
            return Integer.parseInt(String.valueOf(v));
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("'" + key + "' must be an integer, got: " + v);
        }
    }
}
