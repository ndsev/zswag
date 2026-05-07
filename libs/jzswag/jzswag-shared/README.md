# jzswag-shared

Platform-agnostic core of the zswag Java client. Sits between `jzswag-api` (interfaces only) and the platform-specific `jzswag-jvm` / `jzswag-android` modules. Contains every line of code that does not depend on a particular HTTP transport, OS keychain, or logging backend.

## Contents

- **`OpenAPIClient`** — the request-decomposition + dispatch core. Reads `x-zserio-request-part` from the parsed spec, encodes parameters via `ParameterEncoder`, applies security via `applySecurity()`, and hands the final `HttpRequest` off to the injected `IHttpClient`.
- **`OpenAPIParser`** — SnakeYAML-based OpenAPI 3.0 parser, with full support for the zswag extensions (`x-zserio-request-part`, `application/x-zserio-object`, OAuth2 `clientCredentials` flow). Rejects PATCH operations and non-`clientCredentials` OAuth2 flows up front.
- **`ParameterEncoder`** — per-location encoding (`encodeForPath`, `encodeForQuery`, `encodeForHeader`, `encodeForCookie`) covering `simple`/`label`/`matrix`/`form` × `explode` × `string`/`byte`/`base64`/`base64url`/`hex`/`binary`.
- **`OAuth2Handler`** — client-credentials flow with cached, refresh-token-aware token minting. Supports both `rfc6749-client-secret-basic` and `rfc5849-oauth1-signature` token-endpoint authentication. Takes an `IKeychain` so it can resolve a `clientSecretKeychain` reference on either platform.
- **`OAuth1Signature`** — RFC 5849 HMAC-SHA256 signature builder used by the `rfc5849-oauth1-signature` token-endpoint auth method.
- **`HttpSettingsLoader`** — YAML loader for the multi-scope settings file (`HTTP_SETTINGS_FILE`). Schema documented in [`docs/http-settings.md`](../../docs/http-settings.md), shared with the C++ and Python clients.
- **`ZserioReflection`** — POJO getter reflection that resolves dotted `x-zserio-request-part` paths against zserio-Java-generated request structs.
- **`ZswagServiceClient`** — legacy `IZswagServiceClient` adapter for callers that want a method-name-and-bytes interface rather than the typed `ServiceClientInterface` path.

## Dependencies

- `jzswag-api` (peer module, transitive `api` exposure).
- zserio-runtime ≥ 2.16.1.
- SnakeYAML 2.2 (YAML parsing).
- Gson 2.10.1 (OAuth2 token-response JSON).
- SLF4J 2.0.9 API (binding chosen by the consuming platform module).

## Usage

This module is a peer dependency of the platform implementations; you don't depend on it directly. Add either `jzswag-jvm` or `jzswag-android` and you'll get this module transitively.

## Testing

```bash
./gradlew :libs:jzswag:jzswag-shared:test
```

Coverage is ≥60% line on the suite. Unit tests cover the YAML loader, multi-scope merging, parameter encoding, OAuth1 signature conformance, OAuth2 flow edge cases, and zserio reflection.
