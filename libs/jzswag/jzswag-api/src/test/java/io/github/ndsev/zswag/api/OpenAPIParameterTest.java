package io.github.ndsev.zswag.api;

import org.junit.jupiter.api.Test;

import static org.assertj.core.api.Assertions.assertThat;

class OpenAPIParameterTest {

    @Test
    void defaultStyleForPathIsSimple() {
        OpenAPIParameter p = OpenAPIParameter.builder("id", ParameterLocation.PATH).build();
        assertThat(p.getStyle()).isEqualTo(ParameterStyle.SIMPLE);
        assertThat(p.isExplode()).isFalse();
    }

    @Test
    void defaultStyleForHeaderIsSimple() {
        OpenAPIParameter p = OpenAPIParameter.builder("X", ParameterLocation.HEADER).build();
        assertThat(p.getStyle()).isEqualTo(ParameterStyle.SIMPLE);
        assertThat(p.isExplode()).isFalse();
    }

    @Test
    void defaultStyleForQueryIsFormExploded() {
        OpenAPIParameter p = OpenAPIParameter.builder("q", ParameterLocation.QUERY).build();
        assertThat(p.getStyle()).isEqualTo(ParameterStyle.FORM);
        assertThat(p.isExplode()).isTrue();
    }

    @Test
    void defaultStyleForCookieIsFormExploded() {
        OpenAPIParameter p = OpenAPIParameter.builder("c", ParameterLocation.COOKIE).build();
        assertThat(p.getStyle()).isEqualTo(ParameterStyle.FORM);
        assertThat(p.isExplode()).isTrue();
    }

    @Test
    void formatDefaultsToString() {
        OpenAPIParameter p = OpenAPIParameter.builder("x", ParameterLocation.QUERY).build();
        assertThat(p.getFormat()).isEqualTo(ParameterFormat.STRING);
    }

    @Test
    void buildersStoreOverrides() {
        OpenAPIParameter p = OpenAPIParameter.builder("x", ParameterLocation.QUERY)
                .style(ParameterStyle.PIPE_DELIMITED)
                .format(ParameterFormat.HEX)
                .required(true)
                .explode(false)
                .requestPart("base.field")
                .build();
        assertThat(p.getName()).isEqualTo("x");
        assertThat(p.getLocation()).isEqualTo(ParameterLocation.QUERY);
        assertThat(p.getStyle()).isEqualTo(ParameterStyle.PIPE_DELIMITED);
        assertThat(p.getFormat()).isEqualTo(ParameterFormat.HEX);
        assertThat(p.isRequired()).isTrue();
        assertThat(p.isExplode()).isFalse();
        assertThat(p.getRequestPart()).contains("base.field");
        assertThat(p.isWholeRequest()).isFalse();
    }

    @Test
    void wholeRequestSentinelDetected() {
        OpenAPIParameter p = OpenAPIParameter.builder("body", ParameterLocation.QUERY)
                .requestPart(OpenAPIParameter.REQUEST_PART_WHOLE)
                .build();
        assertThat(p.isWholeRequest()).isTrue();
        assertThat(OpenAPIParameter.REQUEST_PART_WHOLE).isEqualTo("*");
    }

    @Test
    void requestPartAbsentWhenNotSet() {
        OpenAPIParameter p = OpenAPIParameter.builder("x", ParameterLocation.QUERY).build();
        assertThat(p.getRequestPart()).isEmpty();
        assertThat(p.isWholeRequest()).isFalse();
    }
}
