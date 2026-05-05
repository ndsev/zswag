# jzswag-test

Integration tests for the Java zswag client. Validates the full dispatch flow against the Python Calculator server (`zswag.test.calc`).

## What's tested

`CalculatorTestClient` exercises 10 cases covering every parameter style, format, and authentication scheme the Calculator API exposes:

| # | Operation | Tests |
|---|---|---|
| 1 | `power(BaseAndExponent)` | nested `x-zserio-request-part` (`base.value`, `exponent.value`); path + header parameters; explicit `security: []` (no auth). |
| 2 | `intSum(Integers)` | `style: form, explode: true` query array; hex-encoded ints; HTTP Bearer auth. |
| 3 | `byteSum(Bytes)` | base64url-encoded byte array in path; HTTP Basic auth. |
| 4 | `intMul(Integers)` | base64-encoded int32 array in path; query API-key auth. |
| 5 | `floatMul(Doubles)` | float array in query (`explode: false`); cookie API-key auth. |
| 6 | `bitMul(Bools)` | bool array; header API-key auth; expects `false`. |
| 7 | `bitMul(Bools)` | bool array; header API-key auth; expects `true`. |
| 8 | `identity(Double)` | POST request body as `application/x-zserio-object`; cookie API-key auth. |
| 9 | `concat(Strings)` | base64-encoded string array; HTTP Bearer auth. |
| 10 | `name(EnumWrapper)` | enum unwrap to numeric via `ZserioEnum.getGenericValue()`; global default `HeaderAuth` security. |

The test client is structured as the **canonical Java port idiom**: each test constructs a `ZswagClient` (which implements `zserio.runtime.service.ServiceClientInterface`), wraps it in the zserio-generated `Calculator.CalculatorClient`, and invokes the typed method directly. There is no manual request decomposition — every parameter is resolved via `x-zserio-request-part`.

## Running the test

### Prerequisites

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install zswag        # the test depends on the Python server as the counterparty
```

### Automated harness

```bash
./libs/jzswag-test/test-java-client.bash
```

The script builds the Java test client, starts the Python Calculator server on port 5555, runs `CalculatorTestClient`, and stops the server on exit.

### Manual

```bash
# In one terminal:
python3 -m zswag.test.calc server localhost:5555

# In another:
./gradlew :libs:jzswag-test:run --args="localhost:5555"
```

## Why this test matters

The earlier "test passing" claim from before the parity work was misleading: the test harness was hand-decomposing each request into the parameter map the OpenAPI spec required, then calling `oaClient.callMethod(path, params, preSerializedBytes)`. The Java client itself never read `x-zserio-request-part`. After the parity rewrite the test now goes through the actual zswag flow, so a green run validates that the Java client genuinely matches the Python/C++ behaviour end-to-end.

## Build notes

The build downloads the zserio Java compiler and generates Java classes from `libs/zswag/test/calc/calculator.zs` on every `compileJava`. A post-codegen sed step in `build.gradle` patches the generated `Calculator.java` to qualify `String` as `java.lang.String` where needed (the calc service has a zserio struct named `String` that shadows `java.lang.String` inside the `calculator` package — a zserio-Java codegen quirk specific to services with that struct name).

## See also

- [`docs/java.md`](../../docs/java.md) — canonical Java client guide.
- [`libs/zswag/test/calc/api.yaml`](../../libs/zswag/test/calc/api.yaml) — the OpenAPI spec the test exercises (good reference for `x-zserio-request-part` usage).
- [`CalculatorTestClient.java`](src/main/java/com/ndsev/zswag/test/CalculatorTestClient.java) — the test source.
