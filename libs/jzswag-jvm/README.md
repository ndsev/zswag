# jzswag-jvm

Pure Java JVM port of the zswag OpenAPI client. Built on the JDK 11 `HttpClient`; no JNI. Runs anywhere a standard JVM does — desktop, server, lambda, CLI, IDE plugin.

## Role in the project

- Implements zserio's `zserio.runtime.service.ServiceClientInterface` via `ZswagClient`, so a zserio-Java-generated `XClient` accepts an instance as its transport — the same idiom as Python's `services.MyService.Client(OAClient(url))` and C++'s `MyService::Client(openApiClient)`.
- Performs full request decomposition driven by the OpenAPI spec's `x-zserio-request-part` extension, with all parameter styles (`simple`/`label`/`matrix`/`form` × `explode`) and formats (`string`/`byte`/`base64`/`base64url`/`hex`/`binary`).
- Handles all authentication schemes: HTTP Basic, HTTP Bearer, API key (header/query/cookie), and OAuth2 client credentials with both `rfc6749-client-secret-basic` and `rfc5849-oauth1-signature` token-endpoint authentication.
- Loads the same `HTTP_SETTINGS_FILE` YAML format the C++ and Python clients use, with URL-scoped persistent settings.
- Integrates with the platform keychain (Linux `secret-tool`, macOS `security`) for credential storage.

## Documentation

See [`docs/java.md`](../../docs/java.md) for the canonical Java client guide — usage idioms, configuration model, OAuth2 wiring, troubleshooting, and the running integration test.

For the OpenAPI feature support matrix (Java vs C++ vs Python), see [the interop tables in README.md](../../README.md#openapi-options-interoperability).

## Module layout

- `ZswagClient` — public entry point; implements `ServiceClientInterface`.
- `JvmOpenAPIClient` — orchestrates `x-zserio-request-part` dispatch and security application.
- `JvmHttpClient` — JDK 11 `HttpClient` wrapper; merges persistent + adhoc config per request; applies SSL/proxy.
- `OpenAPIParser` — parses OpenAPI 3.0 specs with full zswag extensions.
- `ParameterEncoder` — encodes parameter values per location/style/format.
- `ZserioReflection` — resolves `x-zserio-request-part` paths via POJO getter reflection on the typed zserio request object.
- `OAuth2Handler` + `OAuth1Signature` — OAuth2 client-credentials flow with RFC 5849 HMAC-SHA256 signing variant.
- `Keychain` — platform-native keychain shim (Linux `secret-tool`, macOS `security`).
- `HttpSettingsLoader` — YAML loader for the multi-scope settings file.
- `JzswagLogging` — wires `HTTP_LOG_LEVEL` to the logback root logger.

## Dependencies

- `jzswag-api` (peer module).
- zserio-runtime ≥ 2.16.1.
- SnakeYAML 2.2 — YAML parsing.
- Gson 2.10.1 — JSON parsing for OAuth2 token responses.
- SLF4J 2.0.9 + Logback 1.4.14 — logging.

## Testing

```bash
./gradlew :libs:jzswag-jvm:test
```

Unit tests cover the YAML schema, multi-scope merging, parameter encoding, OAuth1 signature conformance, and zserio reflection. Integration testing happens in `libs/jzswag-test/`.
