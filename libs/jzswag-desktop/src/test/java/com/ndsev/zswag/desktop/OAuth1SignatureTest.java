package com.ndsev.zswag.desktop;

import org.junit.jupiter.api.Test;

import java.util.LinkedHashMap;
import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * RFC 5849 + RFC 3986 conformance tests for the OAuth 1.0 HMAC-SHA256
 * signing helper. Exercises the parts that have to byte-for-byte match the
 * C++ {@code httpcl::oauth1::*} implementation so a server validating signed
 * token requests accepts the Java client identically.
 */
class OAuth1SignatureTest {

    @Test
    void percentEncodeKeepsRFC3986Unreserved() {
        // Unreserved per RFC 3986: A-Z a-z 0-9 - . _ ~
        String unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
        assertThat(OAuth1Signature.percentEncode(unreserved)).isEqualTo(unreserved);
    }

    @Test
    void percentEncodeEncodesEverythingElse() {
        // Reserved chars MUST be encoded.
        assertThat(OAuth1Signature.percentEncode("a b")).isEqualTo("a%20b");
        assertThat(OAuth1Signature.percentEncode("x&y=z")).isEqualTo("x%26y%3Dz");
        assertThat(OAuth1Signature.percentEncode("/")).isEqualTo("%2F");
        assertThat(OAuth1Signature.percentEncode("+")).isEqualTo("%2B");
    }

    @Test
    void percentEncodeUsesUpperCaseHex() {
        assertThat(OAuth1Signature.percentEncode("ÿ")).isEqualTo("%C3%BF");
    }

    @Test
    void generateNonceLengthCheckLowerBound() {
        assertThatThrownBy(() -> OAuth1Signature.generateNonce(7))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void generateNonceLengthCheckUpperBound() {
        assertThatThrownBy(() -> OAuth1Signature.generateNonce(65))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void generateNonceProducesAlphanumeric() {
        String n = OAuth1Signature.generateNonce(32);
        assertThat(n).hasSize(32);
        assertThat(n).matches("[A-Za-z0-9]+");
    }

    @Test
    void signatureBaseStringFollowsRFC5849Format() {
        Map<String, String> params = new LinkedHashMap<>();
        params.put("oauth_consumer_key", "key");
        params.put("oauth_nonce", "n");
        params.put("oauth_signature_method", "HMAC-SHA256");
        params.put("oauth_timestamp", "1");
        params.put("oauth_version", "1.0");
        String base = OAuth1Signature.buildSignatureBaseString("POST", "https://x.example.com/oauth/token", params);
        // Sorted, percent-encoded params, joined by &; prefixed by METHOD&percent(URL).
        assertThat(base).isEqualTo(
                "POST&https%3A%2F%2Fx.example.com%2Foauth%2Ftoken&"
                        + "oauth_consumer_key%3Dkey%26"
                        + "oauth_nonce%3Dn%26"
                        + "oauth_signature_method%3DHMAC-SHA256%26"
                        + "oauth_timestamp%3D1%26"
                        + "oauth_version%3D1.0");
    }

    @Test
    void computeSignatureIsDeterministicForFixedInputs() {
        Map<String, String> params = new LinkedHashMap<>();
        params.put("oauth_consumer_key", "client");
        params.put("oauth_nonce", "abc12345");
        params.put("oauth_signature_method", "HMAC-SHA256");
        params.put("oauth_timestamp", "1700000000");
        params.put("oauth_version", "1.0");
        params.put("grant_type", "client_credentials");

        String s1 = OAuth1Signature.computeSignature("POST", "https://issuer/oauth/token", params, "secret", "");
        String s2 = OAuth1Signature.computeSignature("POST", "https://issuer/oauth/token", params, "secret", "");
        assertThat(s1).isEqualTo(s2);
        // base64 of HMAC-SHA256 (32 bytes) is 44 chars including padding.
        assertThat(s1).hasSize(44);
    }

    @Test
    void buildAuthorizationHeaderIncludesAllOauthParams() {
        Map<String, String> bodyParams = new LinkedHashMap<>();
        bodyParams.put("grant_type", "client_credentials");
        String h = OAuth1Signature.buildAuthorizationHeader(
                "POST", "https://issuer/oauth/token", "client", "secret", bodyParams, 16);
        assertThat(h).startsWith("OAuth ");
        assertThat(h)
                .contains("oauth_consumer_key=\"client\"")
                .contains("oauth_signature_method=\"HMAC-SHA256\"")
                .contains("oauth_timestamp=\"")
                .contains("oauth_nonce=\"")
                .contains("oauth_version=\"1.0\"")
                .contains("oauth_signature=\"");
    }
}
