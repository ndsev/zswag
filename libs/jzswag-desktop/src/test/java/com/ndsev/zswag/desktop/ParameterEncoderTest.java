package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;

import java.util.*;

import static org.assertj.core.api.Assertions.*;

/**
 * Unit tests for ParameterEncoder.
 * Tests all parameter styles, formats, and edge cases.
 */
class ParameterEncoderTest {

    // Helper method to create parameters with different configurations
    private OpenAPIParameter param(String name, ParameterLocation location, ParameterStyle style,
                                    ParameterFormat format, boolean explode) {
        return OpenAPIParameter.builder(name, location)
                .style(style)
                .format(format)
                .explode(explode)
                .build();
    }

    @Nested
    @DisplayName("String Format Tests")
    class StringFormatTests {

        @Test
        @DisplayName("Should encode scalar string value")
        void encodeScalarString() {
            OpenAPIParameter p = param("name", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, "hello")).isEqualTo("hello");
        }

        @Test
        @DisplayName("Should encode integer as string")
        void encodeIntegerAsString() {
            OpenAPIParameter p = param("value", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, 42)).isEqualTo("42");
        }

        @Test
        @DisplayName("Should encode negative integer as string")
        void encodeNegativeIntegerAsString() {
            OpenAPIParameter p = param("value", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, -200)).isEqualTo("-200");
        }

        @Test
        @DisplayName("Should encode boolean as 0/1 not true/false")
        void encodeBooleanAsNumeric() {
            OpenAPIParameter p = param("flag", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, true)).isEqualTo("1");
            assertThat(ParameterEncoder.encodeParameter(p, false)).isEqualTo("0");
        }

        @Test
        @DisplayName("Should encode boolean array as 0/1 values")
        void encodeBooleanArrayAsNumeric() {
            OpenAPIParameter p = param("flags", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new boolean[]{true, false, true}))
                    .isEqualTo("1,0,1");
        }
    }

    @Nested
    @DisplayName("Hex Format Tests")
    class HexFormatTests {

        @Test
        @DisplayName("Should encode positive integer as hex without 0x prefix")
        void encodePositiveHex() {
            OpenAPIParameter p = param("value", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.HEX, false);
            assertThat(ParameterEncoder.encodeParameter(p, 100)).isEqualTo("64");
            assertThat(ParameterEncoder.encodeParameter(p, 255)).isEqualTo("ff");
            assertThat(ParameterEncoder.encodeParameter(p, 400)).isEqualTo("190");
        }

        @Test
        @DisplayName("Should encode negative integer as signed hex with minus prefix")
        void encodeNegativeHex() {
            OpenAPIParameter p = param("value", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.HEX, false);
            assertThat(ParameterEncoder.encodeParameter(p, -200)).isEqualTo("-c8");
            assertThat(ParameterEncoder.encodeParameter(p, -1)).isEqualTo("-1");
        }

        @Test
        @DisplayName("Should encode int array as hex values")
        void encodeIntArrayAsHex() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.HEX, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{100, -200, 400}))
                    .isEqualTo("64,-c8,190");
        }

        @Test
        @DisplayName("Should encode byte array as continuous hex")
        void encodeByteArrayAsHex() {
            OpenAPIParameter p = param("data", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.HEX, false);
            assertThat(ParameterEncoder.encodeParameter(p, new byte[]{0x01, 0x02, (byte) 0xFF}))
                    .isEqualTo("0102ff");
        }
    }

    @Nested
    @DisplayName("Base64 Format Tests")
    class Base64FormatTests {

        @Test
        @DisplayName("Should encode string as base64")
        void encodeStringAsBase64() {
            OpenAPIParameter p = param("data", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.BASE64, false);
            assertThat(ParameterEncoder.encodeParameter(p, "foo")).isEqualTo("Zm9v");
            assertThat(ParameterEncoder.encodeParameter(p, "bar")).isEqualTo("YmFy");
        }

        @Test
        @DisplayName("Should encode byte array as base64")
        void encodeByteArrayAsBase64() {
            OpenAPIParameter p = param("data", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.BASE64, false);
            assertThat(ParameterEncoder.encodeParameter(p, new byte[]{1, 2, 3, 4}))
                    .isEqualTo("AQIDBA==");
        }

        @Test
        @DisplayName("Should encode int32 array as base64 with 4 bytes per element")
        void encodeInt32ArrayAsBase64() {
            OpenAPIParameter p = param("values", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.BASE64, false);
            // int32 [1, 2, 3, 4] should be encoded as 4 bytes each in big-endian
            String result = ParameterEncoder.encodeParameter(p, new int[]{1, 2, 3, 4});
            // Each int is 4 bytes: 1 = 0x00000001, etc.
            assertThat(result).isEqualTo("AAAAAQ==,AAAAAg==,AAAAAw==,AAAABA==");
        }

        @Test
        @DisplayName("Should encode String array as base64")
        void encodeStringArrayAsBase64() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.BASE64, false);
            String[] strings = {"foo", "bar"};
            assertThat(ParameterEncoder.encodeParameter(p, strings))
                    .isEqualTo("Zm9v,YmFy");
        }
    }

    @Nested
    @DisplayName("Base64URL Format Tests")
    class Base64UrlFormatTests {

        @Test
        @DisplayName("Should encode uint8 (short[]) as single byte base64url each")
        void encodeUint8ArrayAsBase64Url() {
            OpenAPIParameter p = param("values", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.BASE64URL, false);
            // uint8 values [8, 16, 32, 64] - each is 1 byte
            String result = ParameterEncoder.encodeParameter(p, new short[]{8, 16, 32, 64});
            // 8 = 0x08 -> CA==, 16 = 0x10 -> EA==, 32 = 0x20 -> IA==, 64 = 0x40 -> QA==
            assertThat(result).isEqualTo("CA==,EA==,IA==,QA==");
        }

        @Test
        @DisplayName("Should include padding in base64url")
        void base64UrlIncludesPadding() {
            OpenAPIParameter p = param("value", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.BASE64URL, false);
            // Short (2 bytes) 8 = 0x0008 encodes to "AAg=" with single = padding
            // (2 bytes = 16 bits -> 3 base64 chars + 1 padding char)
            assertThat(ParameterEncoder.encodeParameter(p, (short) 8)).contains("=");
        }

        @Test
        @DisplayName("Should use URL-safe characters")
        void base64UrlUsesUrlSafeChars() {
            OpenAPIParameter p = param("data", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.BASE64URL, false);
            // Test with data that would produce + or / in standard base64
            byte[] data = new byte[]{(byte) 0xFB, (byte) 0xEF}; // Would be ++8 in standard base64
            String result = ParameterEncoder.encodeParameter(p, data);
            assertThat(result).doesNotContain("+").doesNotContain("/");
        }
    }

    @Nested
    @DisplayName("Parameter Style Tests")
    class ParameterStyleTests {

        @Test
        @DisplayName("Simple style - scalar value")
        void simpleStyleScalar() {
            OpenAPIParameter p = param("id", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, "5")).isEqualTo("5");
        }

        @Test
        @DisplayName("Simple style - array value")
        void simpleStyleArray() {
            OpenAPIParameter p = param("ids", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo("3,4,5");
        }

        @Test
        @DisplayName("Label style - scalar value")
        void labelStyleScalar() {
            OpenAPIParameter p = param("id", ParameterLocation.PATH, ParameterStyle.LABEL, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, "5")).isEqualTo(".5");
        }

        @Test
        @DisplayName("Label style - array without explode")
        void labelStyleArrayNoExplode() {
            OpenAPIParameter p = param("ids", ParameterLocation.PATH, ParameterStyle.LABEL, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo(".3,4,5");
        }

        @Test
        @DisplayName("Label style - array with explode")
        void labelStyleArrayExplode() {
            OpenAPIParameter p = param("ids", ParameterLocation.PATH, ParameterStyle.LABEL, ParameterFormat.STRING, true);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo(".3.4.5");
        }

        @Test
        @DisplayName("Matrix style - scalar value")
        void matrixStyleScalar() {
            OpenAPIParameter p = param("id", ParameterLocation.PATH, ParameterStyle.MATRIX, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, "5")).isEqualTo(";id=5");
        }

        @Test
        @DisplayName("Matrix style - array without explode")
        void matrixStyleArrayNoExplode() {
            OpenAPIParameter p = param("ids", ParameterLocation.PATH, ParameterStyle.MATRIX, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo(";ids=3,4,5");
        }

        @Test
        @DisplayName("Matrix style - array with explode")
        void matrixStyleArrayExplode() {
            OpenAPIParameter p = param("ids", ParameterLocation.PATH, ParameterStyle.MATRIX, ParameterFormat.STRING, true);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo(";ids=3;ids=4;ids=5");
        }

        @Test
        @DisplayName("Form style - array value")
        void formStyleArray() {
            OpenAPIParameter p = param("ids", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo("3,4,5");
        }

        @Test
        @DisplayName("Pipe delimited style")
        void pipeDelimitedStyle() {
            OpenAPIParameter p = param("ids", ParameterLocation.QUERY, ParameterStyle.PIPE_DELIMITED, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo("3|4|5");
        }

        @Test
        @DisplayName("Space delimited style")
        void spaceDelimitedStyle() {
            OpenAPIParameter p = param("ids", ParameterLocation.QUERY, ParameterStyle.SPACE_DELIMITED, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{3, 4, 5}))
                    .isEqualTo("3 4 5");
        }
    }

    @Nested
    @DisplayName("URL Encoding Tests")
    class UrlEncodingTests {

        @Test
        @DisplayName("Should URL encode special characters")
        void urlEncodeSpecialChars() {
            assertThat(ParameterEncoder.urlEncode("hello world")).isEqualTo("hello+world");
            assertThat(ParameterEncoder.urlEncode("a=b&c=d")).isEqualTo("a%3Db%26c%3Dd");
            assertThat(ParameterEncoder.urlEncode("foo/bar")).isEqualTo("foo%2Fbar");
        }

        @Test
        @DisplayName("Should build query string from parameters")
        void buildQueryString() {
            Map<String, String> params = new LinkedHashMap<>();
            params.put("name", "John Doe");
            params.put("age", "30");

            String queryString = ParameterEncoder.buildQueryString(params);
            assertThat(queryString).contains("name=John+Doe");
            assertThat(queryString).contains("age=30");
            assertThat(queryString).contains("&");
        }

        @Test
        @DisplayName("Should handle empty parameter map")
        void emptyQueryString() {
            assertThat(ParameterEncoder.buildQueryString(Collections.emptyMap())).isEmpty();
        }
    }

    @Nested
    @DisplayName("Collection Type Tests")
    class CollectionTypeTests {

        @Test
        @DisplayName("Should handle List collection")
        void encodeListCollection() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            List<Integer> list = Arrays.asList(1, 2, 3);
            assertThat(ParameterEncoder.encodeParameter(p, list)).isEqualTo("1,2,3");
        }

        @Test
        @DisplayName("Should handle Set collection")
        void encodeSetCollection() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            Set<String> set = new LinkedHashSet<>(Arrays.asList("a", "b", "c"));
            assertThat(ParameterEncoder.encodeParameter(p, set)).isEqualTo("a,b,c");
        }

        @Test
        @DisplayName("Should handle Object array")
        void encodeObjectArray() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            Object[] arr = {"x", "y", "z"};
            assertThat(ParameterEncoder.encodeParameter(p, arr)).isEqualTo("x,y,z");
        }

        @Test
        @DisplayName("Should handle double array")
        void encodeDoubleArray() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            double[] arr = {1.5, 2.5, 3.5};
            assertThat(ParameterEncoder.encodeParameter(p, arr)).isEqualTo("1.5,2.5,3.5");
        }

        @Test
        @DisplayName("Should handle float array")
        void encodeFloatArray() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            float[] arr = {34.5f, 2.0f};
            assertThat(ParameterEncoder.encodeParameter(p, arr)).isEqualTo("34.5,2.0");
        }

        @Test
        @DisplayName("Should handle long array")
        void encodeLongArray() {
            OpenAPIParameter p = param("values", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.STRING, false);
            long[] arr = {100L, 200L, 300L};
            assertThat(ParameterEncoder.encodeParameter(p, arr)).isEqualTo("100,200,300");
        }

        @Test
        @DisplayName("Should handle empty array")
        void encodeEmptyArray() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            int[] arr = {};
            assertThat(ParameterEncoder.encodeParameter(p, arr)).isEmpty();
        }
    }

    @Nested
    @DisplayName("Edge Cases")
    class EdgeCaseTests {

        @Test
        @DisplayName("Should handle zero value")
        void encodeZero() {
            OpenAPIParameter p = param("value", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.HEX, false);
            assertThat(ParameterEncoder.encodeParameter(p, 0)).isEqualTo("0");
        }

        @Test
        @DisplayName("Should handle large numbers")
        void encodeLargeNumbers() {
            OpenAPIParameter p = param("value", ParameterLocation.PATH, ParameterStyle.SIMPLE, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, Long.MAX_VALUE))
                    .isEqualTo(String.valueOf(Long.MAX_VALUE));
        }

        @Test
        @DisplayName("Should handle single element array")
        void encodeSingleElementArray() {
            OpenAPIParameter p = param("values", ParameterLocation.QUERY, ParameterStyle.FORM, ParameterFormat.STRING, false);
            assertThat(ParameterEncoder.encodeParameter(p, new int[]{42})).isEqualTo("42");
        }
    }
}
