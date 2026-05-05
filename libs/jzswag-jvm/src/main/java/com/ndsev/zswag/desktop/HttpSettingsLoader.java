package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.HttpConfig;
import com.ndsev.zswag.api.HttpSettings;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.yaml.snakeyaml.LoaderOptions;
import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.constructor.SafeConstructor;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
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
