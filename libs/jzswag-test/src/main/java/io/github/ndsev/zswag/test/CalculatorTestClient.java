package io.github.ndsev.zswag.test;

import calculator.BaseAndExponent;
import calculator.Bool;
import calculator.Bools;
import calculator.Bytes;
import calculator.Calculator;
import calculator.Double;
import calculator.Doubles;
import calculator.Enum;
import calculator.EnumWrapper;
import calculator.I32;
import calculator.Integers;
import calculator.Strings;
// NOTE: calculator.String shadows java.lang.String — qualify java strings as java.lang.String.
import io.github.ndsev.zswag.api.HttpConfig;
import io.github.ndsev.zswag.api.HttpSettings;
import io.github.ndsev.zswag.jvm.JvmHttpClient;
import io.github.ndsev.zswag.jvm.JvmOpenAPIClient;
import io.github.ndsev.zswag.jvm.ZswagClient;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Integration test for the Java zswag client against the Python Calculator
 * server. Mirrors the Python {@code libs/zswag/test/calc/client.py} flow.
 *
 * <p>This is the canonical "Java port" usage: each test constructs a
 * {@link ZswagClient}, wraps it in the zserio-generated
 * {@link Calculator.CalculatorClient}, and invokes the typed method directly.
 * No manual request decomposition — every parameter is resolved from the
 * zserio request object via {@code x-zserio-request-part}.
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
        System.exit(client.runAllTests());
    }

    public int runAllTests() {
        java.lang.String serverUrl = java.lang.String.format("http://%s:%d/openapi.json", host, port);
        System.out.printf("[java-test-client] Connecting to %s%n", serverUrl);
        System.out.flush();

        // Test 1: power() — security: [], base in path, exponent in X-Ponent header.
        runTest("Pass fields in path and header (no auth)", () -> {
            BaseAndExponent request = new BaseAndExponent();
            request.setBase(new I32(2));
            request.setExponent(new I32(3));
            request.setUnused1(0);
            request.setUnused2("");
            request.setUnused3(0.0f);
            request.setUnused5(new boolean[0]);

            Calculator.CalculatorClient calc = newCalcClient(serverUrl, HttpConfig.empty());
            Double response = calc.powerMethod(request);
            assertDoubleEquals(8.0, response.getValue(), "power(2, 3) should equal 8");
        });

        // Test 2: intSum() — Bearer auth, hex-encoded array in query, explode: true.
        runTest("Pass hex-encoded array in query (Bearer auth)", () -> {
            Integers request = new Integers(new int[]{100, -200, 400});
            HttpConfig adhoc = HttpConfig.builder()
                    .header("Authorization", "Bearer 123")
                    .build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            Double response = calc.intSumMethod(request);
            assertDoubleEquals(300.0, response.getValue(), "intSum([100, -200, 400]) should equal 300");
        });

        // Test 3: byteSum() — Basic auth, base64url-encoded byte array in path.
        runTest("Pass base64url-encoded byte array in path (Basic auth)", () -> {
            Bytes request = new Bytes(new short[]{8, 16, 32, 64});
            HttpConfig adhoc = HttpConfig.builder().basicAuth("u", "pw").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            Double response = calc.byteSumMethod(request);
            assertDoubleEquals(120.0, response.getValue(), "byteSum([8, 16, 32, 64]) should equal 120");
        });

        // Test 4: intMul() — Query API-key auth, base64-encoded array in path.
        runTest("Pass base64-encoded long array in path (Query API-key)", () -> {
            Integers request = new Integers(new int[]{1, 2, 3, 4});
            HttpConfig adhoc = HttpConfig.builder().query("api-key", "42").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            Double response = calc.intMulMethod(request);
            assertDoubleEquals(24.0, response.getValue(), "intMul([1, 2, 3, 4]) should equal 24");
        });

        // Test 5: floatMul() — Cookie auth, float array in query, explode: false.
        runTest("Pass float array in query (Cookie auth)", () -> {
            Doubles request = new Doubles(new double[]{34.5, 2.0});
            HttpConfig adhoc = HttpConfig.builder().cookie("api-cookie", "42").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            Double response = calc.floatMulMethod(request);
            assertDoubleEquals(69.0, response.getValue(), "floatMul([34.5, 2.0]) should equal 69");
        });

        // Test 6: bitMul() — Header API-key, bool array (false expected).
        runTest("Pass bool array in query (Header API-key, expect false)", () -> {
            Bools request = new Bools(new boolean[]{true, false});
            HttpConfig adhoc = HttpConfig.builder().header("X-Generic-Token", "42").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            Bool response = calc.bitMulMethod(request);
            assertEquals(false, response.getValue(), "bitMul([true, false]) should equal false");
        });

        // Test 7: bitMul() — Header API-key, bool array (true expected).
        runTest("Pass bool array in query (Header API-key, expect true)", () -> {
            Bools request = new Bools(new boolean[]{true, true});
            HttpConfig adhoc = HttpConfig.builder().header("X-Generic-Token", "42").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            Bool response = calc.bitMulMethod(request);
            assertEquals(true, response.getValue(), "bitMul([true, true]) should equal true");
        });

        // Test 8: identity() — Cookie auth, request as application/x-zserio-object body.
        runTest("Pass request as blob in body (Cookie auth)", () -> {
            Double request = new Double(1.0);
            HttpConfig adhoc = HttpConfig.builder().cookie("api-cookie", "42").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            Double response = calc.identityMethod(request);
            assertDoubleEquals(1.0, response.getValue(), "identity(1.0) should equal 1.0");
        });

        // Test 9: concat() — Bearer auth, base64-encoded string array.
        runTest("Pass base64-encoded strings (Bearer auth)", () -> {
            Strings request = new Strings(new java.lang.String[]{"foo", "bar"});
            HttpConfig adhoc = HttpConfig.builder().header("Authorization", "Bearer 123").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            calculator.String response = calc.concatMethod(request);
            assertEquals("foobar", response.getValue(), "concat(['foo', 'bar']) should equal 'foobar'");
        });

        // Test 10: name() — global default Header auth, enum value as path scalar.
        runTest("Pass enum (global default Header auth)", () -> {
            EnumWrapper request = new EnumWrapper(Enum.TEST_ENUM_0);
            HttpConfig adhoc = HttpConfig.builder().header("X-Generic-Token", "42").build();
            Calculator.CalculatorClient calc = newCalcClient(serverUrl, adhoc);
            calculator.String response = calc.nameMethod(request);
            assertEquals("TEST_ENUM_0", response.getValue(), "name(TEST_ENUM_0) should equal 'TEST_ENUM_0'");
        });

        System.out.println();
        if (failedTests > 0) {
            System.out.printf("[java-test-client] Done, %d test(s) failed!%n", failedTests);
            return 1;
        }
        System.out.println("[java-test-client] All tests succeeded!");
        return 0;
    }

    private Calculator.CalculatorClient newCalcClient(java.lang.String openApiUrl, HttpConfig adhoc) throws Exception {
        // No persistent settings file in this test; adhoc carries the auth/headers per call.
        ZswagClient transport = new ZswagClient(openApiUrl, HttpSettings.empty(), adhoc);
        return new Calculator.CalculatorClient(transport);
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
