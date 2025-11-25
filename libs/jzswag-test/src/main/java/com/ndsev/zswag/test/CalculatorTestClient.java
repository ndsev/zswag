package com.ndsev.zswag.test;

import calculator.BaseAndExponent;
import calculator.Bool;
import calculator.Bools;
import calculator.Bytes;
import calculator.Double;
import calculator.Doubles;
import calculator.Enum;
import calculator.EnumWrapper;
import calculator.I32;
import calculator.Integers;
import calculator.Strings;
// NOTE: Use calculator.String fully qualified to avoid conflict with java.lang.String
import com.ndsev.zswag.api.*;
import com.ndsev.zswag.desktop.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import zserio.runtime.io.SerializeUtil;
import zserio.runtime.io.Writer;

import java.time.Duration;
import java.util.HashMap;
import java.util.Map;

/**
 * Integration test client for Calculator service.
 * Tests Java client against Python zswag server.
 * Mirrors the functionality of libs/zswag/test/calc/client.py
 */
public class CalculatorTestClient {
    private static final Logger logger = LoggerFactory.getLogger(CalculatorTestClient.class);

    private final java.lang.String host;
    private final int port;
    private int testCounter = 0;
    private int failedTests = 0;

    public CalculatorTestClient(java.lang.String host, int port) {
        this.host = host;
        this.port = port;
    }

    public static void main(java.lang.String[] args) {
        if (args.length == 0) {
            System.err.println("Usage: CalculatorTestClient <host:port>");
            System.err.println("Example: CalculatorTestClient localhost:5555");
            System.exit(1);
        }

        java.lang.String[] hostPort = args[0].split(":");
        java.lang.String host = hostPort[0];
        int port = hostPort.length > 1 ? Integer.parseInt(hostPort[1]) : 5000;

        CalculatorTestClient client = new CalculatorTestClient(host, port);
        int exitCode = client.runAllTests();
        System.exit(exitCode);
    }

    public int runAllTests() {
        java.lang.String serverUrl = java.lang.String.format("http://%s:%d/openapi.json", host, port);
        System.out.printf("[java-test-client] Connecting to %s%n", serverUrl);
        System.out.flush();

        // Test 1: power() - No auth, base in path, exponent in header
        runTest("Pass fields in path and header", () -> {
            BaseAndExponent request = new BaseAndExponent();
            request.setBase(new I32(2));
            request.setExponent(new I32(3));
            request.setUnused1(0);
            request.setUnused2("");
            request.setUnused3(0.0f);
            request.setUnused5(new boolean[0]);

            // Exponent goes in X-Ponent header per OpenAPI spec
            Double response = callMethod("power",
                    request,
                    HttpSettings.builder()
                            .header("X-Ponent", "3")
                            .build());

            assertDoubleEquals(8.0, response.getValue(), "power(2, 3) should equal 8");
        });

        // Test 2: intSum() - Bearer auth, hex-encoded array in query
        runTest("Pass hex-encoded array in query", () -> {
            Integers request = new Integers(new int[]{100, -200, 400});

            Double response = callMethod("intSum",
                    request,
                    HttpSettings.builder()
                            .bearerToken("123")
                            .build());

            assertDoubleEquals(300.0, response.getValue(), "intSum([100, -200, 400]) should equal 300");
        });

        // Test 3: byteSum() - Basic auth, base64url-encoded byte array in path
        runTest("Pass base64url-encoded byte array in path", () -> {
            Bytes request = new Bytes(new short[]{8, 16, 32, 64});

            Double response = callMethod("byteSum",
                    request,
                    HttpSettings.builder()
                            .basicAuth("u", "pw")
                            .build());

            assertDoubleEquals(120.0, response.getValue(), "byteSum([8, 16, 32, 64]) should equal 120");
        });

        // Test 4: intMul() - Query auth (api-key), base64-encoded array in path
        runTest("Pass base64-encoded long array in path", () -> {
            Integers request = new Integers(new int[]{1, 2, 3, 4});

            Double response = callMethod("intMul",
                    request,
                    HttpSettings.builder()
                            .queryParameter("api-key", "42")
                            .build());

            assertDoubleEquals(24.0, response.getValue(), "intMul([1, 2, 3, 4]) should equal 24");
        });

        // Test 5: floatMul() - Cookie auth, float array in query
        runTest("Pass float array in query", () -> {
            Doubles request = new Doubles(new double[]{34.5, 2.0});

            Double response = callMethod("floatMul",
                    request,
                    HttpSettings.builder()
                            .cookie("api-cookie", "42")
                            .build());

            assertDoubleEquals(69.0, response.getValue(), "floatMul([34.5, 2.0]) should equal 69");
        });

        // Test 6: bitMul() - Header auth, bool array in query (expect false)
        runTest("Pass bool array in query (expect false)", () -> {
            Bools request = new Bools(new boolean[]{true, false});

            Bool response = callMethod("bitMul",
                    request,
                    HttpSettings.builder()
                            .header("X-Generic-Token", "42")
                            .build());

            assertEquals(false, response.getValue(), "bitMul([true, false]) should equal false");
        });

        // Test 7: bitMul() - Header auth, bool array in query (expect true)
        runTest("Pass bool array in query (expect true)", () -> {
            Bools request = new Bools(new boolean[]{true, true});

            Bool response = callMethod("bitMul",
                    request,
                    HttpSettings.builder()
                            .header("X-Generic-Token", "42")
                            .build());

            assertEquals(true, response.getValue(), "bitMul([true, true]) should equal true");
        });

        // Test 8: identity() - Cookie auth, request as blob in body
        runTest("Pass request as blob in body", () -> {
            Double request = new Double(1.0);

            Double response = callMethod("identity",
                    request,
                    HttpSettings.builder()
                            .cookie("api-cookie", "42")
                            .build());

            assertDoubleEquals(1.0, response.getValue(), "identity(1.0) should equal 1.0");
        });

        // Test 9: concat() - Bearer auth, base64-encoded strings
        runTest("Pass base64-encoded strings", () -> {
            Strings request = new Strings(new java.lang.String[]{"foo", "bar"});

            calculator.String response = callMethod("concat",
                    request,
                    HttpSettings.builder()
                            .bearerToken("123")
                            .build());

            assertEquals("foobar", response.getValue(), "concat(['foo', 'bar']) should equal 'foobar'");
        });

        // Test 10: name() - Header auth (global default), enum value
        runTest("Pass enum", () -> {
            EnumWrapper request = new EnumWrapper(Enum.TEST_ENUM_0);

            calculator.String response = callMethod("name",
                    request,
                    HttpSettings.builder()
                            .header("X-Generic-Token", "42")
                            .build());

            assertEquals("TEST_ENUM_0", response.getValue(), "name(TEST_ENUM_0) should equal 'TEST_ENUM_0'");
        });

        // Print summary
        System.out.println();
        if (failedTests > 0) {
            System.out.printf("[java-test-client] Done, %d test(s) failed!%n", failedTests);
            return 1;
        } else {
            System.out.println("[java-test-client] All tests succeeded!");
            return 0;
        }
    }

    private void runTest(java.lang.String description, TestCase testCase) {
        testCounter++;
        try {
            System.out.printf("[java-test-client] Test#%d: %s%n", testCounter, description);
            System.out.flush();

            testCase.run();

            System.out.printf("[java-test-client]   -> Success.%n");
            System.out.flush();

        } catch (Exception e) {
            failedTests++;
            System.out.printf("[java-test-client]   -> ERROR: %s%n",
                    e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName());
            logger.error("Test failed", e);
            System.out.flush();
        }
    }

    @SuppressWarnings("unchecked")
    private <T> T callMethod(java.lang.String path, Object request, HttpSettings settings) throws Exception {
        java.lang.String serverUrl = java.lang.String.format("http://%s:%d/openapi.json", host, port);

        // Create HTTP client with settings
        IHttpClient httpClient = new DesktopHttpClient(settings);

        // Create OpenAPI client
        IOpenAPIClient oaClient = new DesktopOpenAPIClient(serverUrl, httpClient);

        // Serialize request - cast to Writer since all generated zserio classes implement it
        byte[] requestData = SerializeUtil.serializeToBytes((Writer) request);

        // Extract parameters from request object using reflection
        Map<String, Object> params = extractParameters(request);

        // Call method
        byte[] responseData = oaClient.callMethod(path, params, requestData);

        // Deserialize response - determine type from request
        if (request instanceof BaseAndExponent || request instanceof Integers ||
                request instanceof Bytes || request instanceof Doubles || request instanceof Double) {
            return (T) SerializeUtil.deserializeFromBytes(Double.class, responseData);
        } else if (request instanceof Bools) {
            return (T) SerializeUtil.deserializeFromBytes(Bool.class, responseData);
        } else if (request instanceof Strings || request instanceof EnumWrapper) {
            return (T) SerializeUtil.deserializeFromBytes(calculator.String.class, responseData);
        } else {
            throw new IllegalArgumentException("Unknown request type: " + request.getClass());
        }
    }

    private Map<java.lang.String, Object> extractParameters(Object request) throws Exception {
        Map<java.lang.String, Object> params = new HashMap<>();

        // Use reflection to extract fields
        Class<?> clazz = request.getClass();

        // For BaseAndExponent
        if (request instanceof BaseAndExponent) {
            BaseAndExponent bae = (BaseAndExponent) request;
            params.put("base", bae.getBase().getValue());
            params.put("exponent", bae.getExponent().getValue());
        }
        // For Integers, Bytes, Doubles, Bools, Strings - extract values arrays
        else if (request instanceof Integers) {
            params.put("values", ((Integers) request).getValues());
        } else if (request instanceof Bytes) {
            params.put("values", ((Bytes) request).getValues());
        } else if (request instanceof Doubles) {
            params.put("values", ((Doubles) request).getValues());
        } else if (request instanceof Bools) {
            params.put("values", ((Bools) request).getValues());
        } else if (request instanceof Strings) {
            params.put("values", ((Strings) request).getValues());
        } else if (request instanceof EnumWrapper) {
            EnumWrapper ew = (EnumWrapper) request;
            params.put("enum_value", ew.getValue().getValue());
        }
        // For Double (identity) - no parameters, body only
        else if (request instanceof Double) {
            // No parameters for identity
        }

        return params;
    }

    private void assertDoubleEquals(double expected, double actual, java.lang.String message) {
        if (Math.abs(expected - actual) > 0.0001) {
            throw new AssertionError(java.lang.String.format("%s: expected %.4f but got %.4f", message, expected, actual));
        }
    }

    private void assertEquals(Object expected, Object actual, java.lang.String message) {
        if (!expected.equals(actual)) {
            throw new AssertionError(java.lang.String.format("%s: expected '%s' but got '%s'", message, expected, actual));
        }
    }

    @FunctionalInterface
    interface TestCase {
        void run() throws Exception;
    }
}
