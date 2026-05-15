package io.github.ndsev.zswag.android;

import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpRequest;
import io.github.ndsev.zswag.api.HttpResponse;
import io.github.ndsev.zswag.api.HttpSettings;
import io.github.ndsev.zswag.api.IHttpClient;
import io.github.ndsev.zswag.api.IKeychain;
import io.github.ndsev.zswag.shared.HttpSettingsLoader;
import okhttp3.Authenticator;
import okhttp3.Call;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;
import okhttp3.Route;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Proxy;
import java.nio.charset.StandardCharsets;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.X509Certificate;
import java.time.Duration;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.StringJoiner;
import java.util.TreeSet;
import java.util.concurrent.TimeUnit;

/**
 * Android {@link IHttpClient} on top of OkHttp 4. Mirrors {@code JvmHttpClient}'s
 * behaviour exactly so a request configured the same way produces the same
 * wire-level traffic on either platform:
 *
 * <ul>
 *   <li>persistent {@link HttpSettings} (scope-matched against the URL) merged
 *       with the per-call adhoc {@link HttpConfig};</li>
 *   <li>per-request headers from the OpenAPI dispatch layer suppress duplicate
 *       merged-config entries (case-insensitive) so OkHttp doesn't emit double
 *       Authorization / Cookie headers;</li>
 *   <li>basic-auth resolved from cleartext password or {@link IKeychain};</li>
 *   <li>per-URL proxy config builds a one-shot OkHttpClient (matches
 *       {@code JvmHttpClient}'s "rare path" approach);</li>
 *   <li>{@code HTTP_SSL_STRICT} env var + {@link HttpConfig#isSslStrict()}
 *       drive a TrustEverythingManager when relaxed mode is required;</li>
 *   <li>{@code HTTP_TIMEOUT} env var sets the connect / read / write timeout
 *       (default 60 s, matching the C++/JVM clients).</li>
 * </ul>
 */
public class AndroidHttpClient implements IHttpClient {
    private static final Logger logger = LoggerFactory.getLogger(AndroidHttpClient.class);

    private static final int DEFAULT_TIMEOUT_SECONDS = 60;
    private static final MediaType OCTET_STREAM = MediaType.parse("application/octet-stream");

    private final HttpSettings persistentSettings;
    private final IKeychain keychain;
    private final OkHttpClient strictClient;
    private final OkHttpClient permissiveClient;

    /** Loads persistent settings from {@code HTTP_SETTINGS_FILE} and uses an in-memory IKeychain stub. */
    public AndroidHttpClient() {
        this(HttpSettingsLoader.loadFromEnvironment(), (s, u) -> {
            throw new IllegalStateException(
                    "AndroidHttpClient was created without an IKeychain; basic-auth keychain lookup is not available. "
                    + "Pass an AndroidKeychain to the constructor.");
        });
    }

    public AndroidHttpClient(@NotNull HttpSettings persistentSettings) {
        this(persistentSettings, (s, u) -> {
            throw new IllegalStateException(
                    "AndroidHttpClient was created without an IKeychain; basic-auth keychain lookup is not available. "
                    + "Pass an AndroidKeychain to the constructor.");
        });
    }

    public AndroidHttpClient(@NotNull HttpSettings persistentSettings, @NotNull IKeychain keychain) {
        AndroidLogging.init();
        this.persistentSettings = persistentSettings;
        this.keychain = keychain;
        Duration timeout = readTimeoutFromEnv();
        this.strictClient = buildOkHttpClient(timeout, true);
        this.permissiveClient = buildOkHttpClient(timeout, false);
    }

    @Override
    @NotNull
    public HttpSettings getPersistentSettings() {
        return persistentSettings;
    }

    @NotNull
    private static Duration readTimeoutFromEnv() {
        String envTimeout = System.getenv("HTTP_TIMEOUT");
        if (envTimeout != null && !envTimeout.isEmpty()) {
            try {
                return Duration.ofSeconds(Integer.parseInt(envTimeout));
            } catch (NumberFormatException e) {
                logger.warn("Invalid HTTP_TIMEOUT value '{}', using default {}s", envTimeout, DEFAULT_TIMEOUT_SECONDS);
            }
        }
        return Duration.ofSeconds(DEFAULT_TIMEOUT_SECONDS);
    }

    private static boolean envSslStrict() {
        // Match C++ httpcl::HttpLibHttpClient (libs/httpcl/src/http-client.cpp:57-58):
        // any non-empty value enables strict; unset or empty disables. Aligned with
        // JvmHttpClient and the Python client (pyzswagcl) for cross-client parity.
        String env = System.getenv("HTTP_SSL_STRICT");
        return env != null && !env.isEmpty();
    }

    @NotNull
    private static OkHttpClient buildOkHttpClient(@NotNull Duration timeout, boolean sslStrict) {
        OkHttpClient.Builder b = new OkHttpClient.Builder()
                .connectTimeout(timeout.getSeconds(), TimeUnit.SECONDS)
                .readTimeout(timeout.getSeconds(), TimeUnit.SECONDS)
                .writeTimeout(timeout.getSeconds(), TimeUnit.SECONDS)
                .followRedirects(true)
                .followSslRedirects(true);
        if (!sslStrict) {
            installPermissiveSsl(b);
        }
        return b.build();
    }

    private static void installPermissiveSsl(@NotNull OkHttpClient.Builder b) {
        try {
            TrustEverythingManager tm = new TrustEverythingManager();
            SSLContext ctx = SSLContext.getInstance("TLS");
            ctx.init(null, new TrustManager[]{tm}, new java.security.SecureRandom());
            b.sslSocketFactory(ctx.getSocketFactory(), tm);
            b.hostnameVerifier((host, session) -> true);
        } catch (NoSuchAlgorithmException | KeyManagementException e) {
            logger.warn("Failed to install permissive SSLContext: {}", e.getMessage());
        }
    }

    @Override
    @NotNull
    public HttpResponse execute(@NotNull HttpRequest request, @NotNull HttpConfig adhoc) throws HttpException {
        HttpConfig effective = persistentSettings.forUrl(request.getUrl()).mergedWith(adhoc);

        boolean sslStrict = envSslStrict() && effective.isSslStrict();
        OkHttpClient client = sslStrict ? strictClient : permissiveClient;

        if (effective.getProxy().isPresent()) {
            client = buildClientWithProxy(effective.getTimeout(), sslStrict, effective.getProxy().get());
        }

        // Honour the merged HttpConfig's per-request timeout. JvmHttpClient applies this
        // via HttpRequest.Builder#timeout; on OkHttp we derive a client from the pool so
        // the connection cache is shared but callTimeout reflects the per-call value.
        Duration callTimeout = effective.getTimeout();
        if (!callTimeout.equals(readTimeoutFromEnv())) {
            client = client.newBuilder()
                    .callTimeout(callTimeout.getSeconds(), TimeUnit.SECONDS)
                    .build();
        }

        String url = applyQueryParams(request.getUrl(), effective.getQuery());
        logger.debug("Executing {} request to {}", request.getMethod(), url);

        Request.Builder rb = new Request.Builder().url(url);

        // Per-request headers (case-insensitive) win over merged config to avoid duplicates.
        Set<String> perRequestHeaderNames = new TreeSet<>(String.CASE_INSENSITIVE_ORDER);
        for (Map.Entry<String, String> h : request.getHeaders().entrySet()) {
            rb.addHeader(h.getKey(), h.getValue());
            perRequestHeaderNames.add(h.getKey());
        }
        for (Map.Entry<String, List<String>> h : effective.getHeaders().entrySet()) {
            if (perRequestHeaderNames.contains(h.getKey())) continue;
            for (String v : h.getValue()) {
                rb.addHeader(h.getKey(), v);
            }
        }

        // Cookies → single Cookie header (skip if a Cookie header was already set per-request)
        if (!effective.getCookies().isEmpty() && !perRequestHeaderNames.contains("Cookie")) {
            StringJoiner cookieJoiner = new StringJoiner("; ");
            for (Map.Entry<String, String> e : effective.getCookies().entrySet()) {
                cookieJoiner.add(e.getKey() + "=" + e.getValue());
            }
            rb.addHeader("Cookie", cookieJoiner.toString());
        }

        // Basic auth — only when Authorization isn't already set.
        if (effective.getAuth().isPresent()
                && !perRequestHeaderNames.contains("Authorization")
                && !containsHeaderIgnoreCase(effective.getHeaders(), "Authorization")) {
            HttpConfig.BasicAuthentication auth = effective.getAuth().get();
            String password = !auth.password.isEmpty()
                    ? auth.password
                    : keychain.load(auth.keychain, auth.user);
            String credentials = auth.user + ":" + password;
            String encoded = Base64.getEncoder().encodeToString(credentials.getBytes(StandardCharsets.UTF_8));
            rb.addHeader("Authorization", "Basic " + encoded);
        }

        // HTTP method + body.
        String method = request.getMethod().toUpperCase();
        byte[] bodyBytes = request.getBody();
        switch (method) {
            case "GET":
                rb.get();
                break;
            case "POST":
                rb.post(bodyBytes != null ? RequestBody.create(bodyBytes, OCTET_STREAM) : RequestBody.create(new byte[0], null));
                break;
            case "PUT":
                rb.put(bodyBytes != null ? RequestBody.create(bodyBytes, OCTET_STREAM) : RequestBody.create(new byte[0], null));
                break;
            case "DELETE":
                rb.delete(bodyBytes != null ? RequestBody.create(bodyBytes, OCTET_STREAM) : null);
                break;
            default:
                throw new HttpException("Unsupported HTTP method: " + request.getMethod());
        }

        Call call = client.newCall(rb.build());
        try (Response response = call.execute()) {
            int code = response.code();
            byte[] respBody = response.body() != null ? response.body().bytes() : null;
            // Return the first value per header name (OkHttp's response.header(name)
            // returns the *last* value, which would diverge from JvmHttpClient's
            // behaviour). Iterate explicitly.
            Map<String, String> headers = new LinkedHashMap<>();
            for (String name : response.headers().names()) {
                List<String> values = response.headers().values(name);
                if (!values.isEmpty()) {
                    headers.put(name, values.get(0));
                }
            }
            logger.debug("Received response with status code: {}", code);
            return new HttpResponse(code, response.message(), headers, respBody);
        } catch (IOException e) {
            logger.error("HTTP request failed: {}", e.getMessage(), e);
            throw new HttpException("HTTP request failed: " + e.getMessage(), e);
        }
    }

    private OkHttpClient buildClientWithProxy(@NotNull Duration timeout, boolean sslStrict, @NotNull HttpConfig.Proxy proxy) {
        OkHttpClient.Builder b = new OkHttpClient.Builder()
                .connectTimeout(timeout.getSeconds(), TimeUnit.SECONDS)
                .readTimeout(timeout.getSeconds(), TimeUnit.SECONDS)
                .writeTimeout(timeout.getSeconds(), TimeUnit.SECONDS)
                .proxy(new Proxy(Proxy.Type.HTTP, new InetSocketAddress(proxy.host, proxy.port)));
        if (!sslStrict) installPermissiveSsl(b);
        if (!proxy.user.isEmpty()) {
            String password = !proxy.password.isEmpty() ? proxy.password : keychain.load(proxy.keychain, proxy.user);
            String creds = "Basic " + Base64.getEncoder()
                    .encodeToString((proxy.user + ":" + password).getBytes(StandardCharsets.UTF_8));
            b.proxyAuthenticator(new Authenticator() {
                @Override
                @Nullable
                public Request authenticate(@Nullable Route route, @NotNull Response response) {
                    return response.request().newBuilder().header("Proxy-Authorization", creds).build();
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

    private static final class TrustEverythingManager implements X509TrustManager {
        @Override public void checkClientTrusted(X509Certificate[] chain, String authType) {}
        @Override public void checkServerTrusted(X509Certificate[] chain, String authType) {}
        @Override public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
    }
}
