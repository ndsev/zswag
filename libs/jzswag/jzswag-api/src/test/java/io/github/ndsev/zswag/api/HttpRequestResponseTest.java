package io.github.ndsev.zswag.api;

import org.junit.jupiter.api.Test;

import java.util.LinkedHashMap;
import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class HttpRequestResponseTest {

    @Test
    void requestBuilderSetsAllFields() {
        HttpRequest r = HttpRequest.builder()
                .method("GET")
                .url("https://example.com/x")
                .header("X", "y")
                .build();
        assertThat(r.getMethod()).isEqualTo("GET");
        assertThat(r.getUrl()).isEqualTo("https://example.com/x");
        assertThat(r.getHeaders()).containsEntry("X", "y");
        assertThat(r.getBody()).isNull();
    }

    @Test
    void requestBodyIsDefensivelyCopied() {
        byte[] orig = new byte[]{1, 2, 3};
        HttpRequest r = HttpRequest.builder().method("POST").url("u").body(orig).build();
        // Mutating the original must not affect the request body
        orig[0] = 99;
        assertThat(r.getBody()).containsExactly(1, 2, 3);
        // The returned body is also a defensive copy
        byte[] returned = r.getBody();
        returned[0] = 88;
        assertThat(r.getBody()).containsExactly(1, 2, 3);
    }

    @Test
    void requestBuilderHeadersBulkAddsAll() {
        Map<String, String> bulk = new LinkedHashMap<>();
        bulk.put("A", "1");
        bulk.put("B", "2");
        HttpRequest r = HttpRequest.builder().method("GET").url("u").headers(bulk).build();
        assertThat(r.getHeaders()).containsEntry("A", "1").containsEntry("B", "2");
    }

    @Test
    void requestBuilderRequiresMethodAndUrl() {
        assertThatThrownBy(() -> HttpRequest.builder().method("GET").build())
                .isInstanceOf(IllegalStateException.class);
        assertThatThrownBy(() -> HttpRequest.builder().url("u").build())
                .isInstanceOf(IllegalStateException.class);
    }

    @Test
    void requestHeadersAreImmutable() {
        HttpRequest r = HttpRequest.builder().method("GET").url("u").header("a", "b").build();
        assertThatThrownBy(() -> r.getHeaders().put("c", "d"))
                .isInstanceOf(UnsupportedOperationException.class);
    }

    @Test
    void responseStatusCodeAndIsSuccessful() {
        HttpResponse ok = new HttpResponse(200, "OK", null, null);
        HttpResponse created = new HttpResponse(201, null, null, null);
        HttpResponse redirect = new HttpResponse(301, null, null, null);
        HttpResponse notFound = new HttpResponse(404, "Not Found", null, null);
        HttpResponse serverErr = new HttpResponse(500, null, null, null);
        assertThat(ok.isSuccessful()).isTrue();
        assertThat(created.isSuccessful()).isTrue();
        assertThat(redirect.isSuccessful()).isFalse();
        assertThat(notFound.isSuccessful()).isFalse();
        assertThat(serverErr.isSuccessful()).isFalse();
        assertThat(ok.getStatusMessage()).isEqualTo("OK");
        assertThat(notFound.getStatusCode()).isEqualTo(404);
    }

    @Test
    void responseBodyIsDefensivelyCopied() {
        byte[] orig = new byte[]{9, 8, 7};
        HttpResponse r = new HttpResponse(200, null, null, orig);
        orig[0] = 0;
        assertThat(r.getBody()).containsExactly(9, 8, 7);
        byte[] read = r.getBody();
        read[0] = 0;
        assertThat(r.getBody()).containsExactly(9, 8, 7);
    }

    @Test
    void responseHeadersAreImmutable() {
        Map<String, String> headers = new LinkedHashMap<>();
        headers.put("X", "y");
        HttpResponse r = new HttpResponse(200, null, headers, null);
        assertThat(r.getHeaders()).containsEntry("X", "y");
        assertThatThrownBy(() -> r.getHeaders().put("c", "d"))
                .isInstanceOf(UnsupportedOperationException.class);
    }

    @Test
    void responseHandlesNullBodyAndHeaders() {
        HttpResponse r = new HttpResponse(204, null, null, null);
        assertThat(r.getBody()).isNull();
        assertThat(r.getHeaders()).isEmpty();
        assertThat(r.getStatusMessage()).isNull();
    }

    @Test
    void httpExceptionConstructors() {
        HttpException simple = new HttpException("oops");
        assertThat(simple).hasMessage("oops");
        assertThat(simple.getStatusCode()).isNull();
        assertThat(simple.getResponseBody()).isNull();

        Throwable cause = new RuntimeException("root");
        HttpException withCause = new HttpException("err", cause);
        assertThat(withCause.getCause()).isSameAs(cause);

        byte[] body = new byte[]{1, 2};
        HttpException withStatus = new HttpException("bad", 500, body);
        assertThat(withStatus.getStatusCode()).isEqualTo(500);
        body[0] = 99;
        assertThat(withStatus.getResponseBody()).containsExactly(1, 2);
    }

    @Test
    void httpExceptionWithNullResponseBodyIsNull() {
        HttpException e = new HttpException("x", 400, null);
        assertThat(e.getResponseBody()).isNull();
    }
}
