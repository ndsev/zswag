package io.github.ndsev.zswag.jvm;

import io.github.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.ProxySelector;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.X509Certificate;
import java.time.Duration;
import java.util.Base64;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.StringJoiner;
import java.util.TreeSet;

/**
 * JVM {@link IHttpClient} on top of the JDK 11 {@link HttpClient}.
 *
 * <p>On every request the client merges its persistent {@link HttpSettings}
 * (URL-scope-matched) with the adhoc {@link HttpConfig} passed by the caller,
 * matching the C++ {@code HttpLibHttpClient} flow. Headers, cookies, query
 * parameters, basic-auth and proxy from the merged config are applied to the
 * underlying request.
 */
public class JvmHttpClient implements IHttpClient {
    private static final Logger logger = LoggerFactory.getLogger(JvmHttpClient.class);

    private static final int DEFAULT_TIMEOUT_SECONDS = 60;

    private final HttpSettings persistentSettings;
    private final HttpClient strictClient;
    private final HttpClient permissiveClient;

    /**
     * Creates a client that loads persistent settings from {@code HTTP_SETTINGS_FILE}
     * and applies {@code HTTP_TIMEOUT} / {@code HTTP_SSL_STRICT} env vars.
     */
    public JvmHttpClient() {
        this(HttpSettingsLoader.loadFromEnvironment());
    }

    public JvmHttpClient(@NotNull HttpSettings persistentSettings) {
        JzswagLogging.init();
        this.persistentSettings = persistentSettings;
        Duration timeout = readTimeoutFromEnv();
        this.strictClient = buildJdkClient(timeout, true);
        this.permissiveClient = buildJdkClient(timeout, false);
    }

    /** For tests: explicit timeout override. */
    JvmHttpClient(@NotNull HttpSettings persistentSettings, @NotNull Duration timeout) {
        this.persistentSettings = persistentSettings;
        this.strictClient = buildJdkClient(timeout, true);
        this.permissiveClient = buildJdkClient(timeout, false);
    }

    @NotNull
    public HttpSettings getPersistentSettings() {
        return persistentSettings;
    }

    @NotNull
    private static Duration readTimeoutFromEnv() {
        String envTimeout = System.getenv("HTTP_TIMEOUT");
        if (envTimeout != null && !envTimeout.isEmpty()) {
            try {
                int seconds = Integer.parseInt(envTimeout);
                return Duration.ofSeconds(seconds);
            } catch (NumberFormatException e) {
                logger.warn("Invalid HTTP_TIMEOUT value '{}', using default {}s", envTimeout, DEFAULT_TIMEOUT_SECONDS);
            }
        }
        return Duration.ofSeconds(DEFAULT_TIMEOUT_SECONDS);
    }

    private static boolean envSslStrict() {
        String env = System.getenv("HTTP_SSL_STRICT");
        if (env == null || env.isEmpty()) return true;
        return "1".equals(env) || "true".equalsIgnoreCase(env);
    }

    private static HttpClient buildJdkClient(@NotNull Duration connectTimeout, boolean sslStrict) {
        HttpClient.Builder b = HttpClient.newBuilder()
                .version(HttpClient.Version.HTTP_1_1)
                .followRedirects(HttpClient.Redirect.NORMAL)
                .connectTimeout(connectTimeout);
        if (!sslStrict) {
            try {
                SSLContext ctx = SSLContext.getInstance("TLS");
                ctx.init(null, new TrustManager[]{new TrustEverythingManager()}, new java.security.SecureRandom());
                b.sslContext(ctx);
            } catch (NoSuchAlgorithmException | KeyManagementException e) {
                logger.warn("Failed to install permissive SSLContext: {}", e.getMessage());
            }
        }
        return b.build();
    }

    @Override
    @NotNull
    public io.github.ndsev.zswag.api.HttpResponse execute(@NotNull io.github.ndsev.zswag.api.HttpRequest request,
                                                    @NotNull HttpConfig adhoc) throws HttpException {
        // Merge: persistent (scope-matched) | adhoc — matches C++ Settings[uri] |= httpConfig_
        HttpConfig effective = persistentSettings.forUrl(request.getUrl()).mergedWith(adhoc);

        // Effective SSL strictness: request.adhoc has the final say if it ever sets sslStrict=false,
        // otherwise honor env. (Persistent settings file does not carry sslStrict in C++ either.)
        boolean sslStrict = envSslStrict() && effective.isSslStrict();
        HttpClient jdk = sslStrict ? strictClient : permissiveClient;

        // Resolve proxy if configured. JDK HttpClient takes proxy on the client builder, so for
        // configs that vary per-URL we'd need a per-request client; since proxy is rare, build
        // a one-shot client when proxy is set.
        if (effective.getProxy().isPresent()) {
            jdk = buildClientWithProxy(jdk.connectTimeout().orElse(Duration.ofSeconds(DEFAULT_TIMEOUT_SECONDS)),
                    sslStrict, effective.getProxy().get());
        }

        try {
            String url = applyQueryParams(request.getUrl(), effective.getQuery());
            logger.debug("Executing {} request to {}", request.getMethod(), url);

            HttpRequest.Builder rb = HttpRequest.newBuilder()
                    .uri(URI.create(url))
                    .timeout(effective.getTimeout());

            // Per-request headers from the OpenAPI dispatch layer take precedence: any
            // header set here (e.g., OAuth2 Bearer minted by applySecurity) suppresses
            // the same header from the merged persistent + adhoc layer below. This
            // prevents the JDK HttpRequest.Builder.header() append-semantics from
            // emitting duplicate Authorization (or other single-valued) headers when
            // both layers configure them.
            Set<String> perRequestHeaderNames = new TreeSet<>(String.CASE_INSENSITIVE_ORDER);
            for (Map.Entry<String, String> h : request.getHeaders().entrySet()) {
                rb.header(h.getKey(), h.getValue());
                perRequestHeaderNames.add(h.getKey());
            }
            // Persistent + adhoc headers (multi-valued); skip names already supplied above.
            for (Map.Entry<String, List<String>> h : effective.getHeaders().entrySet()) {
                if (perRequestHeaderNames.contains(h.getKey())) continue;
                for (String v : h.getValue()) {
                    rb.header(h.getKey(), v);
                }
            }

            // Cookies → single Cookie header (skip if a Cookie header was already set per-request)
            if (!effective.getCookies().isEmpty() && !perRequestHeaderNames.contains("Cookie")) {
                StringJoiner cookieJoiner = new StringJoiner("; ");
                for (Map.Entry<String, String> e : effective.getCookies().entrySet()) {
                    cookieJoiner.add(e.getKey() + "=" + e.getValue());
                }
                rb.header("Cookie", cookieJoiner.toString());
            }

            // Basic auth — only set if Authorization isn't already provided (e.g., bearer
            // from per-request OAuth2 minting, or static Authorization in effective.headers)
            if (effective.getAuth().isPresent()
                    && !perRequestHeaderNames.contains("Authorization")
                    && !containsHeaderIgnoreCase(effective.getHeaders(), "Authorization")) {
                HttpConfig.BasicAuthentication auth = effective.getAuth().get();
                String password = !auth.password.isEmpty()
                        ? auth.password
                        : Keychain.load(auth.keychain, auth.user);
                String credentials = auth.user + ":" + password;
                String encoded = Base64.getEncoder().encodeToString(credentials.getBytes(StandardCharsets.UTF_8));
                rb.header("Authorization", "Basic " + encoded);
            }

            // HTTP method + body
            switch (request.getMethod().toUpperCase()) {
                case "GET":
                    rb.GET();
                    break;
                case "POST":
                    rb.POST(request.getBody() != null
                            ? HttpRequest.BodyPublishers.ofByteArray(request.getBody())
                            : HttpRequest.BodyPublishers.noBody());
                    break;
                case "PUT":
                    rb.PUT(request.getBody() != null
                            ? HttpRequest.BodyPublishers.ofByteArray(request.getBody())
                            : HttpRequest.BodyPublishers.noBody());
                    break;
                case "DELETE":
                    if (request.getBody() != null) {
                        rb.method("DELETE", HttpRequest.BodyPublishers.ofByteArray(request.getBody()));
                    } else {
                        rb.DELETE();
                    }
                    break;
                default:
                    throw new HttpException("Unsupported HTTP method: " + request.getMethod());
            }

            HttpResponse<byte[]> response = jdk.send(rb.build(), HttpResponse.BodyHandlers.ofByteArray());
            logger.debug("Received response with status code: {}", response.statusCode());

            return new io.github.ndsev.zswag.api.HttpResponse(
                    response.statusCode(),
                    null,
                    convertHeaders(response.headers().map()),
                    response.body());

        } catch (IOException e) {
            logger.error("HTTP request failed: {}", e.getMessage(), e);
            throw new HttpException("HTTP request failed: " + e.getMessage(), e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            logger.error("HTTP request interrupted: {}", e.getMessage(), e);
            throw new HttpException("HTTP request interrupted: " + e.getMessage(), e);
        }
    }

    private static HttpClient buildClientWithProxy(@NotNull Duration timeout, boolean sslStrict, @NotNull HttpConfig.Proxy proxy) {
        HttpClient.Builder b = HttpClient.newBuilder()
                .version(HttpClient.Version.HTTP_1_1)
                .followRedirects(HttpClient.Redirect.NORMAL)
                .connectTimeout(timeout)
                .proxy(ProxySelector.of(new InetSocketAddress(proxy.host, proxy.port)));
        if (!sslStrict) {
            try {
                SSLContext ctx = SSLContext.getInstance("TLS");
                ctx.init(null, new TrustManager[]{new TrustEverythingManager()}, new java.security.SecureRandom());
                b.sslContext(ctx);
            } catch (NoSuchAlgorithmException | KeyManagementException e) {
                logger.warn("Failed to install permissive SSLContext: {}", e.getMessage());
            }
        }
        if (!proxy.user.isEmpty()) {
            String password = !proxy.password.isEmpty() ? proxy.password : Keychain.load(proxy.keychain, proxy.user);
            b.authenticator(new java.net.Authenticator() {
                @Override
                protected java.net.PasswordAuthentication getPasswordAuthentication() {
                    return new java.net.PasswordAuthentication(proxy.user, password.toCharArray());
                }
            });
        }
        return b.build();
    }

    private static boolean containsHeaderIgnoreCase(@NotNull Map<String, List<String>> headers, @NotNull String name) {
        for (String key : headers.keySet()) {
            if (name.equalsIgnoreCase(key)) return true;
        }
        return false;
    }

    @NotNull
    private static String applyQueryParams(@NotNull String baseUrl, @NotNull Map<String, List<String>> query) {
        if (query.isEmpty()) return baseUrl;
        StringBuilder sb = new StringBuilder(baseUrl);
        boolean hasQuery = baseUrl.indexOf('?') >= 0;
        for (Map.Entry<String, List<String>> e : query.entrySet()) {
            for (String v : e.getValue()) {
                sb.append(hasQuery ? '&' : '?');
                hasQuery = true;
                sb.append(java.net.URLEncoder.encode(e.getKey(), StandardCharsets.UTF_8));
                sb.append('=');
                sb.append(java.net.URLEncoder.encode(v, StandardCharsets.UTF_8));
            }
        }
        return sb.toString();
    }

    @NotNull
    private static Map<String, String> convertHeaders(@NotNull Map<String, List<String>> headersMap) {
        Map<String, String> result = new java.util.LinkedHashMap<>();
        for (Map.Entry<String, List<String>> e : headersMap.entrySet()) {
            if (!e.getValue().isEmpty()) {
                result.put(e.getKey(), e.getValue().get(0));
            }
        }
        return result;
    }

    private static final class TrustEverythingManager implements X509TrustManager {
        @Override public void checkClientTrusted(X509Certificate[] chain, String authType) {}
        @Override public void checkServerTrusted(X509Certificate[] chain, String authType) {}
        @Override public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
    }
}
