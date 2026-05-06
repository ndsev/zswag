# jzswag-api

Platform-agnostic types and interfaces shared by every other Java module (`jzswag-shared`, `jzswag-jvm`, `jzswag-android`).

## Contents

- **`HttpConfig`** — per-request adhoc HTTP configuration (headers, query, cookies, basic-auth, proxy, OAuth2, API key). Mirrors C++ `httpcl::Config` and Python `HTTPConfig`. Immutable; build via `HttpConfig.builder()`.
- **`HttpSettings`** — multi-scope persistent settings registry (URL pattern → `HttpConfig`). Mirrors C++ `httpcl::Settings`. Loaded from `HTTP_SETTINGS_FILE` by `HttpSettingsLoader` in `jzswag-shared`.
- **`OpenAPIParameter`**, **`ParameterLocation`**, **`ParameterStyle`**, **`ParameterFormat`** — model types for OpenAPI 3.0 parameter encoding, including the zswag-specific `x-zserio-request-part` extension.
- **`SecurityScheme`**, **`SecuritySchemeType`**, **`SecurityRequirement`** — model types for the OpenAPI security flow, preserving OR-of-AND alternatives.
- **`IHttpClient`** — HTTP transport interface; impls apply persistent + adhoc config per request and expose `getPersistentSettings()` so the dispatch core can compute the effective config without downcasting.
- **`IKeychain`** — credential-store interface; impls live in the platform modules (`Keychain` on JVM, `AndroidKeychain` on Android) and are injected into `OAuth2Handler` and the platform HTTP clients.
- **`HttpRequest`**, **`HttpResponse`**, **`HttpException`** — request/response value types and the standard exception type for non-200 responses, connection failures, and timeouts.

## Dependencies

- Java 11+
- zserio-runtime 2.16.1+

No third-party dependencies (the YAML loader for `HttpSettings` lives in `jzswag-shared` to keep this module dep-free).

## Usage

This module is a peer dependency of the platform implementations; you don't depend on it directly. See [`docs/java.md`](../../docs/java.md) for client usage examples.
