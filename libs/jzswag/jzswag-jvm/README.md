# jzswag-jvm

JVM port of the zswag OpenAPI client. Built on the JDK 11 `HttpClient`; no JNI. Runs anywhere a standard JVM does — server, desktop, lambda, CLI, IDE plugin. Pulls in `jzswag-shared` for the platform-agnostic core (OpenAPI dispatch, parameter encoding, OAuth2 flow, YAML loader); only the HTTP transport, keychain, and logging are JVM-specific.

## Role in the project

- Implements zserio's `zserio.runtime.service.ServiceClientInterface` via `OAClient`, so a zserio-Java-generated `XClient` accepts an instance as its transport — the same idiom as Python's `services.MyService.Client(OAClient(url))` and C++'s `MyService::Client(openApiClient)`.
- Performs full request decomposition driven by the OpenAPI spec's `x-zserio-request-part` extension (logic in `jzswag-shared`).
- Handles all authentication schemes: HTTP Basic, HTTP Bearer, API key (header/query/cookie), OAuth2 client credentials with both `rfc6749-client-secret-basic` and `rfc5849-oauth1-signature` token-endpoint methods.
- Loads the same `HTTP_SETTINGS_FILE` YAML format as the C++ and Python clients, with URL-scoped persistent settings.
- Integrates with the platform keychain (Linux `secret-tool`, macOS `security`) for credential storage.

## Documentation

See [`docs/java.md`](../../docs/java.md) for the canonical Java client guide — usage idioms, configuration model, OAuth2 wiring, troubleshooting, and the running integration test.

For the OpenAPI feature support matrix (Java vs C++ vs Python), see [the interop tables in README.md](../../README.md#openapi-options-interoperability).

## JVM-specific contents

- `OAClient` — public entry point; implements `ServiceClientInterface`. Constructs a `JvmHttpClient` + `Keychain` and delegates to the shared `OpenApiClient`.
- `JvmHttpClient` — JDK 11 `HttpClient` wrapper; merges persistent + adhoc config per request; applies SSL/proxy/basic-auth/cookies.
- `Keychain` — `IKeychain` impl that shells out to platform tools: Linux `secret-tool`, macOS `security`. Windows lookup is not yet implemented.
- `JzswagLogging` — wires `HTTP_LOG_LEVEL` env var to the logback root logger via reflection.

(All the cross-platform pieces — `OpenApiClient`, `OpenAPIParser`, `ParameterEncoder`, `ZserioReflection`, `OAuth2Handler`, `OAuth1Signature`, `HttpSettingsLoader` — live in `jzswag-shared`.)

## Dependencies

- `jzswag-shared` (transitively pulls `jzswag-api`, zserio-runtime, SnakeYAML, Gson, slf4j-api).
- Logback 1.4.14 (runtime SLF4J binding).

## Testing

```bash
./gradlew :libs:jzswag:jzswag-jvm:test
```

Line coverage ≥60%. Unit tests cover header / cookie / query / basic-auth merging via OkHttp's `MockWebServer`, the `Keychain` OS-detection branches, and the `JzswagLogging` init paths. Integration testing happens in `libs/jzswag/jzswag-test/`.
