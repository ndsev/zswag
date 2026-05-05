package com.ndsev.zswag.desktop;

import org.jetbrains.annotations.NotNull;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.security.InvalidKeyException;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * OAuth 1.0 (RFC 5849) signature utilities — HMAC-SHA256 only. Java port of
 * C++ {@code httpcl::oauth1::*}. Used for the
 * {@code rfc5849-oauth1-signature} variant of OAuth2 token-endpoint
 * authentication.
 */
public final class OAuth1Signature {
    private static final SecureRandom RANDOM = new SecureRandom();
    private static final char[] ALPHANUM =
            ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz").toCharArray();

    private OAuth1Signature() {}

    /** Cryptographically secure alphanumeric nonce of the given length (8..64). */
    @NotNull
    public static String generateNonce(int length) {
        if (length < 8 || length > 64) {
            throw new IllegalArgumentException("Nonce length must be between 8 and 64");
        }
        StringBuilder sb = new StringBuilder(length);
        for (int i = 0; i < length; i++) {
            sb.append(ALPHANUM[RANDOM.nextInt(ALPHANUM.length)]);
        }
        return sb.toString();
    }

    /** Seconds-since-epoch as decimal string. */
    @NotNull
    public static String generateTimestamp() {
        return Long.toString(System.currentTimeMillis() / 1000L);
    }

    /**
     * RFC 3986 percent-encoding: keep unreserved characters (A-Z, a-z, 0-9, -, ., _, ~);
     * percent-encode everything else as upper-case hex.
     */
    @NotNull
    public static String percentEncode(@NotNull String input) {
        byte[] bytes = input.getBytes(StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder(bytes.length * 3);
        for (byte b : bytes) {
            int u = b & 0xFF;
            if ((u >= 'A' && u <= 'Z')
                    || (u >= 'a' && u <= 'z')
                    || (u >= '0' && u <= '9')
                    || u == '-' || u == '.' || u == '_' || u == '~') {
                sb.append((char) u);
            } else {
                sb.append('%');
                sb.append(Character.toUpperCase(Character.forDigit((u >> 4) & 0xF, 16)));
                sb.append(Character.toUpperCase(Character.forDigit(u & 0xF, 16)));
            }
        }
        return sb.toString();
    }

    /**
     * Builds the signature base string per RFC 5849 Section 3.4.1:
     * {@code METHOD&percent(URL)&percent(sorted-percent-encoded-params)}.
     */
    @NotNull
    static String buildSignatureBaseString(@NotNull String httpMethod, @NotNull String url,
                                           @NotNull Map<String, String> params) {
        List<String> encodedPairs = new ArrayList<>(params.size());
        for (Map.Entry<String, String> e : params.entrySet()) {
            encodedPairs.add(percentEncode(e.getKey()) + "=" + percentEncode(e.getValue()));
        }
        Collections.sort(encodedPairs);
        StringBuilder paramString = new StringBuilder();
        for (int i = 0; i < encodedPairs.size(); i++) {
            if (i > 0) paramString.append('&');
            paramString.append(encodedPairs.get(i));
        }
        return httpMethod.toUpperCase(Locale.ROOT) + "&" + percentEncode(url) + "&" + percentEncode(paramString.toString());
    }

    /**
     * Computes HMAC-SHA256 signature for a token request.
     * Signing key is {@code percent(consumer_secret)&percent(token_secret)}; for the
     * client-credentials flow {@code token_secret} is empty.
     */
    @NotNull
    public static String computeSignature(@NotNull String httpMethod, @NotNull String url,
                                          @NotNull Map<String, String> params,
                                          @NotNull String consumerSecret, @NotNull String tokenSecret) {
        String base = buildSignatureBaseString(httpMethod, url, params);
        String key = percentEncode(consumerSecret) + "&" + percentEncode(tokenSecret);
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(key.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
            byte[] hmac = mac.doFinal(base.getBytes(StandardCharsets.UTF_8));
            return Base64.getEncoder().encodeToString(hmac);
        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            throw new IllegalStateException("HMAC-SHA256 unavailable: " + e.getMessage(), e);
        }
    }

    /**
     * Builds the {@code Authorization: OAuth ...} header for a signed token
     * request, including all five {@code oauth_*} parameters and the computed
     * signature. Body parameters are included in signature computation but are
     * NOT echoed in the header.
     */
    @NotNull
    public static String buildAuthorizationHeader(
            @NotNull String httpMethod, @NotNull String url,
            @NotNull String consumerKey, @NotNull String consumerSecret,
            @NotNull Map<String, String> bodyParams, int nonceLength) {
        String timestamp = generateTimestamp();
        String nonce = generateNonce(nonceLength);

        Map<String, String> allParams = new LinkedHashMap<>();
        allParams.put("oauth_consumer_key", consumerKey);
        allParams.put("oauth_signature_method", "HMAC-SHA256");
        allParams.put("oauth_timestamp", timestamp);
        allParams.put("oauth_nonce", nonce);
        allParams.put("oauth_version", "1.0");
        allParams.putAll(bodyParams);

        String signature = computeSignature(httpMethod, url, allParams, consumerSecret, "");

        StringBuilder h = new StringBuilder("OAuth ");
        h.append("oauth_consumer_key=\"").append(percentEncode(consumerKey)).append("\", ");
        h.append("oauth_signature_method=\"HMAC-SHA256\", ");
        h.append("oauth_timestamp=\"").append(timestamp).append("\", ");
        h.append("oauth_nonce=\"").append(percentEncode(nonce)).append("\", ");
        h.append("oauth_version=\"1.0\", ");
        h.append("oauth_signature=\"").append(percentEncode(signature)).append("\"");
        return h.toString();
    }
}
