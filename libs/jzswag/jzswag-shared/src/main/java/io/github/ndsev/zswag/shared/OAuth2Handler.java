package io.github.ndsev.zswag.shared;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpException;
import io.github.ndsev.zswag.api.HttpRequest;
import io.github.ndsev.zswag.api.HttpResponse;
import io.github.ndsev.zswag.api.IHttpClient;
import io.github.ndsev.zswag.api.IKeychain;
import org.jetbrains.annotations.NotNull;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantLock;

/**
 * OAuth 2.0 client-credentials flow handler with full zswag parity:
 * <ul>
 *   <li>Multi-instance token cache keyed by {@code (tokenUrl, clientId, audience, scopeKey)}
 *       so multiple OAuth2 schemes don't collide.</li>
 *   <li>Refresh-token reuse on expiry; falls back to fresh mint if the refresh fails.</li>
 *   <li>{@code rfc6749-client-secret-basic} (default, HTTP Basic) and
 *       {@code rfc5849-oauth1-signature} (HMAC-SHA256) token-endpoint
 *       authentication methods.</li>
 *   <li>Optional {@code audience} parameter on the token request.</li>
 *   <li>Public client support: when no client secret is configured, the client_id
 *       is sent in the token request body instead.</li>
 *   <li>Override precedence: settings.tokenUrl/refreshUrl/scopes win over spec values.</li>
 * </ul>
 *
 * <p>Mirrors C++ {@code OAuth2ClientCredentialsHandler::satisfy} +
 * {@code requestToken} in {@code openapi-oauth.cpp}.
 */
public final class OAuth2Handler {
    private static final Logger logger = LoggerFactory.getLogger(OAuth2Handler.class);

    private static final String GRANT_TYPE_CLIENT_CREDENTIALS = "client_credentials";
    private static final String GRANT_TYPE_REFRESH_TOKEN = "refresh_token";

    /** Process-wide token cache. Per-handler caches were tested and rejected: in the C++ reference
     * the handler is shared across calls to the same OAClient, and tokens are keyed by
     * (tokenUrl, clientId, audience, scope) so multiple schemes don't collide. */
    private static final ConcurrentHashMap<TokenKey, MintedToken> CACHE = new ConcurrentHashMap<>();
    /** Striped lock pool to serialise mint/refresh attempts. A fixed pool bounds memory
     * regardless of how many distinct {@link TokenKey}s flow through the process; two
     * unrelated keys may occasionally share a stripe (false sharing), which only blocks
     * unrelated mints — an acceptable trade-off for the leak-free behaviour. */
    private static final int LOCK_STRIPES = 32;
    private static final ReentrantLock[] STRIPED_LOCKS = new ReentrantLock[LOCK_STRIPES];
    static {
        for (int i = 0; i < LOCK_STRIPES; i++) STRIPED_LOCKS[i] = new ReentrantLock();
    }

    private static ReentrantLock lockFor(@NotNull TokenKey key) {
        return STRIPED_LOCKS[(key.hashCode() & 0x7fffffff) % LOCK_STRIPES];
    }

    private final IHttpClient httpClient;
    private final IKeychain keychain;
    private final Gson gson = new Gson();

    public OAuth2Handler(@NotNull IHttpClient httpClient, @NotNull IKeychain keychain) {
        this.httpClient = httpClient;
        this.keychain = keychain;
    }

    /**
     * Returns a valid bearer token for the given OAuth2 config + resolved
     * tokenUrl/refreshUrl/scopes (already merged from settings vs spec by the
     * caller). Uses the process-wide cache; mints or refreshes as needed.
     *
     * @throws HttpException if the token endpoint returns non-2xx or the
     *                       response is malformed.
     */
    @NotNull
    public String getAccessToken(@NotNull HttpConfig.OAuth2 oauth, @NotNull String tokenUrl,
                                  @NotNull String refreshUrl, @NotNull List<String> scopes) throws HttpException {
        String scopeKey = String.join(":", scopes);
        TokenKey key = new TokenKey(tokenUrl, oauth.clientId, oauth.audience, scopeKey);

        // Fast path: cached and valid.
        MintedToken cached = CACHE.get(key);
        if (cached != null && System.nanoTime() < cached.expiresAtNanos) {
            logger.debug("[OAuth2] Using cached token (still valid)");
            return cached.accessToken;
        }

        ReentrantLock lock = lockFor(key);
        lock.lock();
        try {
            // Recheck after acquiring lock.
            cached = CACHE.get(key);
            if (cached != null && System.nanoTime() < cached.expiresAtNanos) {
                return cached.accessToken;
            }

            // Try refresh first if we have a refresh token.
            if (cached != null && !cached.refreshToken.isEmpty()) {
                logger.debug("[OAuth2] Cached token expired, attempting refresh at {}...", refreshUrl);
                try {
                    MintedToken refreshed = requestToken(oauth, refreshUrl, GRANT_TYPE_REFRESH_TOKEN,
                            scopes, cached.refreshToken);
                    CACHE.put(key, refreshed);
                    logger.debug("[OAuth2] Refresh successful");
                    return refreshed.accessToken;
                } catch (HttpException e) {
                    logger.debug("[OAuth2] Refresh failed: {}; falling back to mint", e.getMessage());
                }
            }

            // Mint fresh.
            logger.debug("[OAuth2] Minting new token at {}", tokenUrl);
            MintedToken minted = requestToken(oauth, tokenUrl, GRANT_TYPE_CLIENT_CREDENTIALS, scopes, "");
            CACHE.put(key, minted);
            return minted.accessToken;
        } finally {
            lock.unlock();
        }
    }

    /**
     * Performs a single token mint or refresh. {@code refreshToken} is empty
     * for client_credentials grant; non-empty for refresh_token grant.
     */
    @NotNull
    private MintedToken requestToken(@NotNull HttpConfig.OAuth2 oauth, @NotNull String url,
                                     @NotNull String grantType, @NotNull List<String> scopes,
                                     @NotNull String refreshToken) throws HttpException {
        // Build form body.
        StringBuilder body = new StringBuilder("grant_type=").append(grantType);
        if (GRANT_TYPE_CLIENT_CREDENTIALS.equals(grantType)) {
            if (!scopes.isEmpty()) {
                body.append("&scope=").append(ParameterEncoder.urlEncode(String.join(" ", scopes)));
            }
            if (!oauth.audience.isEmpty()) {
                body.append("&audience=").append(ParameterEncoder.urlEncode(oauth.audience));
            }
        } else if (GRANT_TYPE_REFRESH_TOKEN.equals(grantType)) {
            body.append("&refresh_token=").append(ParameterEncoder.urlEncode(refreshToken));
        }

        // Resolve client secret (cleartext or keychain).
        String secret = oauth.clientSecret;
        if (secret.isEmpty() && !oauth.clientSecretKeychain.isEmpty()) {
            secret = keychain.load(oauth.clientSecretKeychain, oauth.clientId);
        }

        // Public client (no secret): send client_id in the body.
        if (secret.isEmpty()) {
            body.append("&client_id=").append(ParameterEncoder.urlEncode(oauth.clientId));
        }

        // Build the HTTP request with the appropriate Authorization scheme.
        HttpRequest.Builder rb = HttpRequest.builder()
                .method("POST")
                .url(url)
                .header("Content-Type", "application/x-www-form-urlencoded")
                .body(body.toString().getBytes(StandardCharsets.UTF_8));

        if (!secret.isEmpty()) {
            switch (oauth.tokenEndpointAuthMethod) {
                case RFC5849_OAUTH1_SIGNATURE: {
                    Map<String, String> bodyParams = parseBodyParams(body.toString());
                    String authHeader = OAuth1Signature.buildAuthorizationHeader(
                            "POST", url, oauth.clientId, secret, bodyParams, oauth.nonceLength);
                    rb.header("Authorization", authHeader);
                    logger.debug("[OAuth2] Token endpoint auth method: rfc5849-oauth1-signature (HMAC-SHA256)");
                    break;
                }
                case RFC6749_CLIENT_SECRET_BASIC:
                default: {
                    String creds = oauth.clientId + ":" + secret;
                    String b64 = java.util.Base64.getEncoder().encodeToString(creds.getBytes(StandardCharsets.UTF_8));
                    rb.header("Authorization", "Basic " + b64);
                    logger.debug("[OAuth2] Token endpoint auth method: rfc6749-client-secret-basic (HTTP Basic)");
                    break;
                }
            }
        }

        logger.debug("[OAuth2] Requesting token: grant_type={}, url={}", grantType, url);

        HttpResponse response = httpClient.execute(rb.build(), HttpConfig.empty());
        if (response.getStatusCode() < 200 || response.getStatusCode() >= 300) {
            String err = response.getBody() != null
                    ? new String(response.getBody(), StandardCharsets.UTF_8)
                    : "(empty)";
            throw new HttpException("OAuth2 token endpoint returned non-2xx (" + response.getStatusCode()
                    + ") for grant_type=" + grantType + ": " + err,
                    response.getStatusCode(), response.getBody());
        }

        byte[] bodyBytes = response.getBody();
        if (bodyBytes == null || bodyBytes.length == 0) {
            throw new HttpException("OAuth2 token endpoint returned 2xx with empty body for grant_type="
                    + grantType, response.getStatusCode(), bodyBytes);
        }
        String responseBody = new String(bodyBytes, StandardCharsets.UTF_8);
        JsonObject json = gson.fromJson(responseBody, JsonObject.class);

        if (json == null || !json.has("access_token")) {
            throw new HttpException("OAuth2: access_token missing in response for grant_type=" + grantType);
        }

        MintedToken minted = new MintedToken();
        minted.accessToken = json.get("access_token").getAsString();
        int expiresIn = json.has("expires_in") ? json.get("expires_in").getAsInt() : 3600;
        // 30-second jiggle to match C++; clamp the floor at 1s so a short-lived test
        // token (expires_in < 30) doesn't go straight into the past and trigger an
        // infinite re-mint loop.
        long effectiveLifetime = Math.max(expiresIn - 30, 1);
        // Use monotonic clock (matches C++ openapi-oauth.cpp:56 which uses std::chrono::steady_clock):
        // wall-clock jumps from NTP slews or manual time changes must not retroactively expire
        // valid tokens or extend the lifetime of an expired one.
        minted.expiresAtNanos = System.nanoTime() + TimeUnit.SECONDS.toNanos(effectiveLifetime);
        if (json.has("refresh_token")) {
            minted.refreshToken = json.get("refresh_token").getAsString();
        } else if (GRANT_TYPE_REFRESH_TOKEN.equals(grantType) && !refreshToken.isEmpty()) {
            // Server didn't reissue; keep the old refresh token.
            minted.refreshToken = refreshToken;
        }
        logger.debug("[OAuth2] Token minted (expires in {}s)", expiresIn);
        return minted;
    }

    @NotNull
    static Map<String, String> parseBodyParams(@NotNull String body) {
        Map<String, String> out = new LinkedHashMap<>();
        if (body.isEmpty()) return out;
        for (String pair : body.split("&")) {
            int eq = pair.indexOf('=');
            if (eq < 0) continue;
            String k = pair.substring(0, eq);
            String v;
            try {
                v = URLDecoder.decode(pair.substring(eq + 1), StandardCharsets.UTF_8);
            } catch (IllegalArgumentException e) {
                v = pair.substring(eq + 1);
            }
            out.put(k, v);
        }
        return out;
    }

    /**
     * Clears the cached token for the given key — call when a 401 is received
     * to force a re-mint on the next request.
     */
    public static void clearToken(@NotNull String tokenUrl, @NotNull String clientId,
                                  @NotNull String audience, @NotNull List<String> scopes) {
        CACHE.remove(new TokenKey(tokenUrl, clientId, audience, String.join(":", scopes)));
    }

    /** Test hook: clears the entire process-wide cache. */
    static void clearAllCachedTokens() {
        CACHE.clear();
    }

    private static final class TokenKey {
        final String tokenUrl;
        final String clientId;
        final String audience;
        final String scopeKey;

        TokenKey(String tokenUrl, String clientId, String audience, String scopeKey) {
            this.tokenUrl = tokenUrl;
            this.clientId = clientId;
            this.audience = audience;
            this.scopeKey = scopeKey;
        }

        @Override public boolean equals(Object o) {
            if (!(o instanceof TokenKey)) return false;
            TokenKey k = (TokenKey) o;
            return Objects.equals(tokenUrl, k.tokenUrl)
                    && Objects.equals(clientId, k.clientId)
                    && Objects.equals(audience, k.audience)
                    && Objects.equals(scopeKey, k.scopeKey);
        }

        @Override public int hashCode() {
            return Objects.hash(tokenUrl, clientId, audience, scopeKey);
        }
    }

    private static final class MintedToken {
        String accessToken = "";
        String refreshToken = "";
        /** Monotonic-clock deadline. Compare via {@code System.nanoTime() < expiresAtNanos}. */
        long expiresAtNanos = 0L;
    }
}
