# Java Client

The Java port of the zswag client ships in two flavours: **JVM** (`jzswag-jvm`, for servers / desktops / CLIs / lambdas) and **Android** (`jzswag-android`). Both implement zserio's `ServiceClientInterface`, so a zserio-Java-generated `XClient` accepts either one as its transport — the same idiom as Python's `services.MyService.Client(OAClient(url))`.

## Modules

| Module | Role |
|---|---|
| `jzswag-api` | Platform-agnostic contracts: `HttpConfig`, `HttpSettings`, `OpenAPIParameter`, `SecurityScheme`, `IHttpClient`, `IKeychain`. No third-party dependencies. |
| `jzswag-shared` | Portable core: `OpenAPIClient` (request decomposition + dispatch), `OpenAPIParser`, `ParameterEncoder`, `OAuth2Handler`, `OAuth1Signature` (RFC 5849 HMAC-SHA256 token-endpoint auth), `HttpSettingsLoader`. Used by both platform modules. |
| `jzswag-jvm` | JVM platform module on top of the JDK 11 `HttpClient`. Provides `OAClient`, `JvmHttpClient`, `Keychain` (Linux `secret-tool` / macOS `security`). |
| `jzswag-android` | Android platform module on top of OkHttp. Provides `OAClient`, `AndroidHttpClient`, `AndroidKeychain` (Android Keystore + AES-GCM-encrypted SharedPreferences). |
| `jzswag-test` | Cross-stack integration tests (Java client ↔ Python Calculator server). |

## Requirements

- **Java 11+** (source/target)
- **Gradle 7+** (the wrapper is committed)
- The zserio Java generator must run on your service's `.zs` files. No special flags required — zswag uses POJO getter reflection on the generated classes, not zserio's `withReflectionCode` / `withTypeInfoCode` (which zserio-Java doesn't yet expose at runtime).

## Quick start

```bash
./gradlew :libs:jzswag:jzswag-jvm:build       # or :libs:jzswag:jzswag-android:build
```

In your project, depend on the platform module that matches your target:

```gradle
dependencies {
    // JVM (server / desktop / CLI / lambda)
    implementation project(':libs:jzswag:jzswag-jvm')

    // OR — Android
    implementation project(':libs:jzswag:jzswag-android')

    implementation "io.github.ndsev:zserio-runtime:2.16.1"
}
```

The platform module pulls in `jzswag-shared` and `jzswag-api` transitively. Both platforms expose the same `OAClient` API; on Android the constructor takes a `Context` so that `AndroidKeychain` can reach `SharedPreferences`.

(Until artifacts are published to Maven Central, depend on the source modules.)

## The canonical idiom

Given a zserio service like:

```
package services;

struct Request { int32 value; };
struct Response { int32 value; };

service MyService {
  Response myApi(Request);
};
```

Run zserio-Java codegen on `services.zs`, then:

```java
// JVM
import io.github.ndsev.zswag.jvm.OAClient;
import services.MyService;

OAClient transport = new OAClient("http://localhost:5000/openapi.json");
MyService.MyServiceClient client = new MyService.MyServiceClient(transport);

Response r = client.myApiMethod(new Request(42));
```

```java
// Android — same idiom, plus a Context for AndroidKeychain
import io.github.ndsev.zswag.android.OAClient;
import services.MyService;

OAClient transport = new OAClient(context, "http://localhost:5000/openapi.json");
MyService.MyServiceClient client = new MyService.MyServiceClient(transport);

Response r = client.myApiMethod(new Request(42));
```

`OAClient` implements `zserio.runtime.service.ServiceClientInterface`. The zserio-generated `XClient` constructor (in this case `MyServiceClient`) accepts that interface, so the wiring is symmetric with Python's `MyService.Client(OAClient(url))` and C++'s `MyService::Client(openApiClient)`.

## Configuration model

Two types describe HTTP configuration:

- **`HttpConfig`** — per-request adhoc config: extra headers, query parameters, cookies, basic-auth, proxy, OAuth2, API key. Mirrors C++ `httpcl::Config` and Python `HTTPConfig`. Immutable; build via `HttpConfig.builder()`.
- **`HttpSettings`** — multi-scope persistent registry, loaded from `HTTP_SETTINGS_FILE`. Each entry has a URL scope (glob pattern); for a given request URL, all matching entries are merged into one effective `HttpConfig`. Mirrors C++ `httpcl::Settings`.

The merge rule on a request: `effective = persistentSettings.forUrl(url) | adhocConfig`. Multi-valued fields (headers, query) union; scalar fields (auth, proxy, oauth2, apiKey) take from the right-hand operand if present.

## Persistent HTTP settings

Set the environment variable `HTTP_SETTINGS_FILE` to point at a YAML file in the format documented in [HTTP Settings File](../README.md#http-settings-file) in the README. The file format is shared with the Python and C++ clients — the same file works for all three.

```yaml
http-settings:
  - scope: https://*.api.example.com/*
    basic-auth:
      user: alice
      keychain: example-api-secret
    headers:
      X-Trace: enabled

  - scope: "https://*.dev.example.com/*"
    oauth2:
      clientId: my-client-id
      clientSecretKeychain: dev-oauth-secret
      tokenUrl: https://issuer.example.com/oauth/token
      scope: ["api.read", "api.write"]
```

Settings are loaded automatically on `JvmHttpClient` construction:

```java
OAClient transport = new OAClient(specUrl);  // reads HTTP_SETTINGS_FILE
```

To pass an explicit settings registry:

```java
HttpSettings settings = HttpSettingsLoader.loadFromFile(Paths.get("custom.yaml"));
OAClient transport = new OAClient(specUrl, settings);
```

To layer a per-instance adhoc config on top:

```java
HttpConfig adhoc = HttpConfig.builder()
    .header("X-Request-Id", UUID.randomUUID().toString())
    .build();
OAClient transport = new OAClient(specUrl, settings, adhoc);
```

## Authentication

zswag honours the `securitySchemes` declared in the OpenAPI spec. The relevant credentials must be present in the merged config (persistent + adhoc); otherwise the dispatch throws a descriptive `HttpException` before sending the request.

| Scheme type | Configure via |
|---|---|
| HTTP Basic | `HttpConfig.basicAuth(user, password)` or `basic-auth` in YAML (with `password` or `keychain`) |
| HTTP Bearer | `HttpConfig.bearerToken(token)` (sets `Authorization: Bearer …`) |
| API key in header | API-key in YAML with the scheme's matching name; auto-routed to the right header |
| API key in cookie | Same — auto-routed into the `Cookie` header |
| API key in query | Same — auto-routed into the URL query |
| OAuth2 (client credentials) | YAML `oauth2:` block — see below |

### OAuth2

zswag supports the OAuth2 `clientCredentials` flow only (matching C++/Python). Other flows in the spec are rejected at parse time.

```yaml
http-settings:
  - scope: https://api.example.com/*
    oauth2:
      clientId: my-client
      clientSecretKeychain: my-oauth-secret   # OR clientSecret: cleartext
      tokenUrl: https://issuer.example.com/oauth/token  # overrides spec value
      audience: https://api.example.com/      # optional (some providers require)
      scope: ["read", "write"]                # overrides per-operation spec scopes
      useForSpecFetch: true                   # acquire token before fetching openapi.json (default)
      tokenEndpointAuth:
        method: rfc6749-client-secret-basic   # or rfc5849-oauth1-signature
        nonceLength: 16                       # for OAuth1 signature (8..64)
```

The handler caches tokens in-process keyed by `(tokenUrl, clientId, audience, scopeKey)`, refreshes via `refresh_token` when present, and falls back to a fresh mint if refresh fails.

For the OAuth1-signature variant, the request to the token endpoint is signed with HMAC-SHA256 per RFC 5849 (used by some providers that require signed token requests).

For public clients (no client secret), simply omit `clientSecret` and `clientSecretKeychain`. The `client_id` is then sent in the request body.

### Keychain

To store credentials in the OS keychain rather than cleartext:

- **Linux**: store with `secret-tool store --label='zswag dev secret' package lib.openapi.zserio.client service my-service user my-user`, reference as `keychain: my-service`.
- **macOS**: store with `security add-generic-password -s my-service -a my-user -w 'thepassword'`, reference as `keychain: my-service`.
- **Windows**: not yet implemented; use cleartext `password:` for now.

Keychain lookups happen lazily when the request is dispatched. Failures (tool missing on PATH, no entry, timeout) raise `KeychainException` with a clear message.

## How request decomposition works

zswag's defining feature is the `x-zserio-request-part` extension: each OpenAPI parameter declares which field of the zserio request it carries (e.g. `base.value`, or `*` for the whole serialized blob). On dispatch, `OAClient`:

1. Looks up the OpenAPI operation by `methodName`.
2. For each declared parameter, resolves its `x-zserio-request-part` path against the typed zserio request via JavaBean getter reflection (`getBase().getValue()`). zserio enums are unwrapped to their numeric value via `ZserioEnum.getGenericValue()`.
3. Encodes each value per the parameter's `format` (string/hex/base64/base64url/byte/binary) and `style`/`explode`, into path / query / header / cookie.
4. If the operation declares an `application/x-zserio-object` request body, serializes the whole request via `Writer.write(BitStreamWriter)`.
5. Applies the `Authorization` header, cookies, and query keys driven by the operation's `security:` requirements.
6. Sends the request; expects HTTP 200 (strict); deserializes the response via the zserio-generated client.

zserio Java field naming matters here: a `.zs` field `enum_value` becomes `getEnumValue()` in Java; the reflection layer normalises snake_case → lowerCamel automatically. If your zserio source uses unconventional naming, verify the `x-zserio-request-part` paths resolve via `ZserioReflection.toGetterName(...)`.

## Environment variables

| Variable | Effect |
|---|---|
| `HTTP_SETTINGS_FILE` | Path to YAML settings file. Empty/unset → no persistent config. |
| `HTTP_TIMEOUT` | Request connection+transfer timeout in seconds. Default `60`. |
| `HTTP_SSL_STRICT` | `0`/`false` disables certificate verification. Default `1`. |
| `HTTP_LOG_LEVEL` | `debug` / `trace` for OAuth2 flow logging. Maps to logback root level. |
| `HTTP_LOG_FILE` / `HTTP_LOG_FILE_MAXSIZE` | Not yet wired in Java — configure logback directly via `logback.xml` for now. |

## Error handling

Non-200 responses raise `HttpException` carrying the status code, response body, and a context string with method + URL. Connection failures and timeouts also surface as `HttpException`.

Strict 200 matches C++; if your service uses 204 or 206 successfully, catch `HttpException` and inspect `getStatusCode()`.

## OpenAPI feature support

The Java client matches the C++/Python clients in feature coverage. See [the interop matrix in README.md](../README.md#openapi-options-interoperability) for the exhaustive ✅/❌ table across all clients.

Highlights:
- HTTP `GET`, `POST`, `PUT`, `DELETE` (no `PATCH` — design constraint, applies to all zswag clients).
- All `x-zserio-request-part` forms: whole-blob (`*`), scalar, array. Compound `x-zserio-request-part` is unsupported by all clients.
- All formats: `string`, `byte`, `base64`, `base64url`, `hex`, `binary`.
- All array styles: `simple`, `label`, `matrix`, `form` × `explode: true|false`.
- Server URL base path resolution (single `servers[0]`).
- All security schemes: HTTP Basic, HTTP Bearer, API key (cookie/header/query), OAuth2 client credentials with both `rfc6749-client-secret-basic` and `rfc5849-oauth1-signature` token-endpoint auth. OpenID Connect is not supported (unsupported across all zswag clients).

## Running the integration test

```bash
# 1. Install the Python wheel for the test server (any zswag release works)
python3 -m venv .venv && source .venv/bin/activate
pip install zswag

# 2. Run the test harness
./libs/jzswag/jzswag-test/test-java-client.bash
```

The script starts the Python `zswag.test.calc` server on port 5555, builds the Java client, and runs `CalculatorTestClient` end-to-end. All 10 tests should pass.

## Troubleshooting

**`zswag.test.calc` not found**: install the Python wheel into your active venv (`pip install zswag`) — the integration test depends on it as the counterparty server.

**Gradle wrapper missing**: bootstrap with `gradle wrapper --gradle-version 9.2.1`. The repo currently includes `gradle-wrapper.properties` only.

**`Required parameter ... resolved to null via x-zserio-request-part`**: the path in the OpenAPI spec doesn't resolve to a non-null field on the zserio request object. Check that the field name matches (snake_case in the OpenAPI side maps to lowerCamel via `getXxx`).

**`OAuth2 client-credentials: tokenUrl is missing in spec and http-settings`**: the spec didn't declare `flows.clientCredentials.tokenUrl` AND your `http-settings.yaml` doesn't override `oauth2.tokenUrl`. Provide one.

**`keychain: 'secret-tool' is not installed or not on PATH`**: install `libsecret-tools` (Linux) or use cleartext `password:` for non-production setups.

## Looking deeper

- [HTTP Settings File in README.md](../README.md#http-settings-file) — full spec of the HTTP_SETTINGS_FILE YAML format, shared with Python and C++ clients.
- [`../libs/jzswag/jzswag-test/src/main/java/com/ndsev/zswag/test/CalculatorTestClient.java`](../libs/jzswag/jzswag-test/src/main/java/com/ndsev/zswag/test/CalculatorTestClient.java) — exhaustive working examples covering each parameter style, format, and authentication scheme.
- [`../libs/zswag/test/calc/api.yaml`](../libs/zswag/test/calc/api.yaml) — the OpenAPI spec the integration test uses; useful reference for what `x-zserio-request-part` looks like in practice.
