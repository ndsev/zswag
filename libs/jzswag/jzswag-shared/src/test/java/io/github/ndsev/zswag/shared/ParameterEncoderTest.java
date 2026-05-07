package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.OpenAPIParameter;
import io.github.ndsev.zswag.api.ParameterFormat;
import io.github.ndsev.zswag.api.ParameterLocation;
import io.github.ndsev.zswag.api.ParameterStyle;
import org.junit.jupiter.api.Test;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;

/**
 * Verifies that ParameterEncoder produces the same byte/string sequences as
 * the C++ {@code openapi-parameter-helper.cpp}, especially for the styles +
 * formats combinations the calc API exercises.
 */
class ParameterEncoderTest {

    @Test
    void scalarPathSimple() {
        OpenAPIParameter p = OpenAPIParameter.builder("base", ParameterLocation.PATH)
                .style(ParameterStyle.SIMPLE).build();
        assertThat(ParameterEncoder.encodeForPath(p, 2)).isEqualTo("2");
    }

    @Test
    void scalarPathLabel() {
        OpenAPIParameter p = OpenAPIParameter.builder("base", ParameterLocation.PATH)
                .style(ParameterStyle.LABEL).build();
        assertThat(ParameterEncoder.encodeForPath(p, "x")).isEqualTo(".x");
    }

    @Test
    void scalarPathMatrix() {
        OpenAPIParameter p = OpenAPIParameter.builder("base", ParameterLocation.PATH)
                .style(ParameterStyle.MATRIX).build();
        assertThat(ParameterEncoder.encodeForPath(p, 7)).isEqualTo(";base=7");
    }

    @Test
    void simpleArrayPathCommaJoined() {
        OpenAPIParameter p = OpenAPIParameter.builder("values", ParameterLocation.PATH)
                .style(ParameterStyle.SIMPLE).format(ParameterFormat.STRING).build();
        assertThat(ParameterEncoder.encodeForPath(p, new int[]{1, 2, 3})).isEqualTo("1,2,3");
    }

    @Test
    void labelArrayWithExplodeUsesDotSeparator() {
        OpenAPIParameter p = OpenAPIParameter.builder("values", ParameterLocation.PATH)
                .style(ParameterStyle.LABEL).explode(true).format(ParameterFormat.STRING).build();
        assertThat(ParameterEncoder.encodeForPath(p, new int[]{1, 2, 3})).isEqualTo(".1.2.3");
    }

    @Test
    void matrixArrayWithExplodeRepeatsName() {
        OpenAPIParameter p = OpenAPIParameter.builder("values", ParameterLocation.PATH)
                .style(ParameterStyle.MATRIX).explode(true).format(ParameterFormat.STRING).build();
        assertThat(ParameterEncoder.encodeForPath(p, new int[]{1, 2})).isEqualTo(";values=1;values=2");
    }

    @Test
    void formArrayExplodeFalseProducesSinglePair() {
        OpenAPIParameter p = OpenAPIParameter.builder("values", ParameterLocation.QUERY)
                .style(ParameterStyle.FORM).explode(false).format(ParameterFormat.STRING).build();
        List<Map.Entry<String, String>> pairs = ParameterEncoder.encodeForQuery(p, new int[]{1, 2, 3});
        assertThat(pairs).hasSize(1);
        assertThat(pairs.get(0).getKey()).isEqualTo("values");
        assertThat(pairs.get(0).getValue()).isEqualTo("1,2,3");
    }

    @Test
    void formArrayExplodeTrueProducesOnePairPerElement() {
        OpenAPIParameter p = OpenAPIParameter.builder("values", ParameterLocation.QUERY)
                .style(ParameterStyle.FORM).explode(true).format(ParameterFormat.STRING).build();
        List<Map.Entry<String, String>> pairs = ParameterEncoder.encodeForQuery(p, Arrays.asList("a", "b", "c"));
        assertThat(pairs).extracting(Map.Entry::getKey).containsExactly("values", "values", "values");
        assertThat(pairs).extracting(Map.Entry::getValue).containsExactly("a", "b", "c");
    }

    @Test
    void hexFormatSignedNegativesUseDashPrefix() {
        OpenAPIParameter p = OpenAPIParameter.builder("v", ParameterLocation.QUERY)
                .format(ParameterFormat.HEX).build();
        List<Map.Entry<String, String>> pairs = ParameterEncoder.encodeForQuery(p, -200);
        assertThat(pairs.get(0).getValue()).isEqualTo("-c8");
    }

    @Test
    void base64FormatUsesByteWidthForIntArray() {
        // int[] elements are 4 bytes each → AAAAAQ== for value 1.
        OpenAPIParameter p = OpenAPIParameter.builder("v", ParameterLocation.PATH)
                .style(ParameterStyle.SIMPLE).format(ParameterFormat.BASE64).build();
        String encoded = ParameterEncoder.encodeForPath(p, new int[]{1, 2});
        assertThat(encoded).isEqualTo("AAAAAQ==,AAAAAg==");
    }

    @Test
    void base64UrlFormatForByteArrayUsesPaddedUrlSafe() {
        // short[] elements are 1 byte (zserio uint8 stored as short).
        OpenAPIParameter p = OpenAPIParameter.builder("v", ParameterLocation.PATH)
                .style(ParameterStyle.SIMPLE).format(ParameterFormat.BASE64URL).build();
        String encoded = ParameterEncoder.encodeForPath(p, new short[]{8, 16, 32, 64});
        // Base64URL of single bytes 8/16/32/64.
        assertThat(encoded).isEqualTo("CA==,EA==,IA==,QA==");
    }

    @Test
    void base64FormatScalarStringEncodesUtf8Bytes() {
        OpenAPIParameter p = OpenAPIParameter.builder("v", ParameterLocation.QUERY)
                .format(ParameterFormat.BASE64).build();
        List<Map.Entry<String, String>> pairs = ParameterEncoder.encodeForQuery(p, "foo");
        assertThat(pairs.get(0).getValue()).isEqualTo("Zm9v");
    }

    @Test
    void booleanScalarFormattedAsZeroOrOne() {
        OpenAPIParameter p = OpenAPIParameter.builder("v", ParameterLocation.QUERY)
                .format(ParameterFormat.STRING).build();
        assertThat(ParameterEncoder.encodeForQuery(p, true).get(0).getValue()).isEqualTo("1");
        assertThat(ParameterEncoder.encodeForQuery(p, false).get(0).getValue()).isEqualTo("0");
    }

    @Test
    void buildQueryStringPreservesOrderAndDuplicates() {
        List<Map.Entry<String, String>> pairs = Arrays.asList(
                new java.util.AbstractMap.SimpleImmutableEntry<>("id", "1"),
                new java.util.AbstractMap.SimpleImmutableEntry<>("id", "2"),
                new java.util.AbstractMap.SimpleImmutableEntry<>("name", "x y")
        );
        // 'x y' must be url-encoded.
        assertThat(ParameterEncoder.buildQueryString(pairs)).isEqualTo("id=1&id=2&name=x+y");
    }
}
