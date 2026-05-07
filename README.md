# Zswag

[![CI](https://github.com/ndsev/zswag/actions/workflows/build-deploy.yml/badge.svg)](https://github.com/ndsev/zswag/actions/workflows/build-deploy.yml)
[![codecov](https://codecov.io/github/ndsev/zswag/graph/badge.svg?token=5DTX2M8IDE)](https://codecov.io/github/ndsev/zswag)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=ndsev_zswag&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=ndsev_zswag)
[![Release](https://img.shields.io/github/release/ndsev/zswag)](https://GitHub.com/ndsev/zswag/releases/)
[![Coverage](https://sonarcloud.io/api/project_badges/measure?project=ndsev_zswag&metric=coverage)](https://sonarcloud.io/summary/new_code?id=ndsev_zswag)
[![License](https://img.shields.io/github/license/ndsev/zswag)](https://github.com/ndsev/zswag/blob/master/LICENSE)

zswag is a set of libraries for using and hosting [zserio](http://zserio.org) services through OpenAPI/REST. It provides parallel client implementations in Python, C++, and Java that consume the same OpenAPI specification, plus a Python server layer for exposing zserio services.

## Components

![Component overview](doc/zswag-architecture.png)

| Component | Language | Role |
|---|---|---|
| `zswagcl` | C++ | Core OpenAPI client (`OAClient`, `OpenApiClient`, `OpenApiConfig`); reused by the Python client via pybind11. |
| `httpcl` | C++ | HTTP wrapper around [cpp-httplib](https://github.com/yhirose/cpp-httplib); request configuration; OS keychain integration via [`keychain`](https://github.com/hrantzsch/keychain). |
| `zswag` | Python | Python `OAClient`, the Flask/Connexion-based `OAServer`, and the `zswag.gen` OpenAPI generator. |
| `pyzswagcl` | Python | pybind11 bindings exposing `zswagcl` to Python. **Internal.** |
| `jzswag-api` | Java | Platform-agnostic types (`HttpConfig`, `HttpSettings`, `OpenAPIParameter`, …). |
| `jzswag-jvm` | Java | Pure-Java port (no JNI) using JDK 11 `HttpClient`. Runs on any standard JVM (server, desktop, lambda). Implements zserio's `ServiceClientInterface`. |
| `jzswag-android` | Java | Android implementation (planned). |

## Per-language documentation

Detailed guides for each client + the server + the generator:

- [`docs/python.md`](docs/python.md) — Python `OAClient` and `OAServer`.
- [`docs/cpp.md`](docs/cpp.md) — C++ client and CMake integration.
- [`docs/java.md`](docs/java.md) — Java client.
- [`docs/openapi-generator.md`](docs/openapi-generator.md) — `zswag.gen` CLI reference.
- [`docs/http-settings.md`](docs/http-settings.md) — Shared YAML format for `HTTP_SETTINGS_FILE` (used by all three clients).

## Quick start

### Python

```bash
pip install zswag
```

```python
from zswag import OAClient
import services.api as services

client = services.MyService.Client(OAClient("http://localhost:5000/openapi.json"))
client.my_api(services.Request(1))
```

### C++

In your `CMakeLists.txt`:

```cmake
FetchContent_Declare(zswag
    GIT_REPOSITORY https://github.com/ndsev/zswag.git
    GIT_TAG v1.11.1)
FetchContent_MakeAvailable(zswag)

add_zserio_library(myapp-zserio-cpp
    WITH_REFLECTION
    ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    ENTRY services.zs
    TOP_LEVEL_PKG myapp_services)

target_link_libraries(myapp myapp-zserio-cpp zswagcl)
```

```cpp
auto httpClient = std::make_unique<httpcl::HttpLibHttpClient>();
auto config = zswagcl::fetchOpenAPIConfig("http://localhost:5000/openapi.json", *httpClient);
auto transport = zswagcl::OAClient(config, std::move(httpClient));
auto client = MyService::Client(transport);
auto resp = client.myApiMethod(Request(1));
```

### Java

```gradle
dependencies {
    implementation project(':libs:jzswag-jvm')
    implementation "io.github.ndsev:zserio-runtime:2.16.1"
}
```

```java
import io.github.ndsev.zswag.jvm.ZswagClient;

ZswagClient transport = new ZswagClient("http://localhost:5000/openapi.json");
MyService.MyServiceClient client = new MyService.MyServiceClient(transport);
Response r = client.myApiMethod(new Request(1));
```

## Setup details

### Python users

Wheels are published for 64-bit Python 3.10–3.13 on Linux (x86_64), macOS (x86_64 / arm64), and Windows (x64). On Windows install the [Microsoft Visual C++ Redistributable](https://aka.ms/vs/16/release/vc_redist.x64.exe).

### C++ users

zswag uses CMake's `FetchContent` for dependencies; CMake ≥ 3.22.3 required. See [`docs/cpp.md`](docs/cpp.md) for full build options including offline / disconnected builds and code coverage.

### Java users

Java 11+ source/target. The integration test depends on `pip install zswag` for its counterparty server. See [`docs/java.md`](docs/java.md).

## CI/CD and Release Process

The project uses GitHub Actions for automated build and deploy:

- **Platforms**: Linux (x86_64), macOS (Intel x86_64 and Apple Silicon arm64), Windows (x64).
- **Python versions**: 3.10, 3.11, 3.12, 3.13.
- **Triggers**: Pull requests, pushes to main, version tags.

### Release process

1. Update `ZSWAG_VERSION` in `CMakeLists.txt` (and the matching version in root `build.gradle`).
2. Tag commit with `v{version}` (e.g. `v1.11.1`).
3. CI validates that the tag version matches the CMake version and deploys wheels to PyPI.

### Development snapshots

Pushes to `main` create development releases — version format `{base_version}.dev{commit_count}` (e.g. `1.11.1.dev3`) — automatically deployed to PyPI for testing.

## Client environment variables

| Variable | Effect |
|---|---|
| `HTTP_SETTINGS_FILE` | Path to YAML settings file (see [`docs/http-settings.md`](docs/http-settings.md)). Empty/unset → no persistent config. |
| `HTTP_LOG_LEVEL` | Verbosity (`debug`, `trace`). Useful for OAuth2 troubleshooting. |
| `HTTP_LOG_FILE` | Logfile path with rotation (Python/C++); not yet wired in Java. |
| `HTTP_LOG_FILE_MAXSIZE` | Rotation size in bytes; default 1 GB (Python/C++ only). |
| `HTTP_TIMEOUT` | Request timeout (connect + transfer) in seconds. Default 60. |
| `HTTP_SSL_STRICT` | Non-empty value enables strict SSL certificate validation. |

## Result code handling

All clients treat any HTTP response other than `200` as an error and raise/throw a typed exception with a descriptive message. To accept other codes (e.g. `204 No Content`), catch the exception and inspect its status code.

## Swagger UI

If `pip install "connexion[swagger-ui]"` is available, `OAServer` exposes API docs at `[/prefix]/ui`.

## OpenAPI Options Interoperability

The Server, Clients, and Generator support different subsets of OpenAPI. The tables below detail which feature is supported by which component. Differences are mostly due to limited development scope — open an issue if you need something missing.

For options not supported by `zswag.gen`, edit the OpenAPI YAML by hand. You'll also need to edit it manually for spec-level metadata (provider name, service version, etc.).

### HTTP method

To change the HTTP method, place the desired method name as the key under the method path:

```yaml
paths:
  /methodName:
    {get|post|put|delete}:
      ...
```

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| `get` `post` `put` `delete` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `patch` | ❌️ | ❌️ | ❌️ | ❌️ | ❌️ |

`patch` is intentionally unsupported across the stack: the partial-object-update semantics it implies cannot be realised in the zserio transport layer interface.

### Request body

Set `requestBody/content` to `application/x-zserio-object` to instruct clients to send the zserio request object in the body when using `post`/`put`/`delete`:

```yaml
requestBody:
  content:
    application/x-zserio-object:
      schema:
        type: string
```

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| `application/x-zserio-object` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Blob Parameter

`x-zserio-request-part: "*"` indicates a parameter holds the whole zserio request as a blob:

```yaml
parameters:
- in: query|path|header
  name: parameterName
  required: true
  x-zserio-request-part: "*"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

About `format`:
- `string` and `binary` produce a raw URL-encoded buffer.
- `byte` and `base64` produce standard Base64.
- `base64url` is URL-safe Base64.
- `hex` is hexadecimal.

When a parameter is in `path`, its value must not be empty (also applies to arrays).

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| `x-zserio-request-part: *` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `format: string` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `format: byte` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `format: hex` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Scalar Parameter

`x-zserio-request-part` can also point to a scalar (nested) member of the request:

```yaml
parameters:
- in: query|path|header
  name: parameterName
  required: true
  x-zserio-request-part: "[parent.]*member"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

For integer types, hex is the natural numeric representation, not binary.

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| `x-zserio-request-part: <[parent.]*member>` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Array Parameter

`x-zserio-request-part` can point to an array member:

```yaml
parameters:
- in: query|path|header
  style: form|simple|label|matrix
  explode: true|false
  name: parameterName
  required: true
  x-zserio-request-part: "[parent.]*array_member"
  schema:
    format: string|byte|base64|base64url|hex|binary
```

The array is encoded according to `format`, `style`, and `explode` per [the OpenAPI 3.1 spec](https://github.com/OAI/OpenAPI-Specification/blob/master/versions/3.1.0.md#parameter-object).

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| `x-zserio-request-part: <[parent.]*array_member>` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `style: simple` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `style: form` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `style: label` | ✔️ | ✔️ | ✔️ | ❌ | ✔️ |
| `style: matrix` | ✔️ | ✔️ | ✔️ | ❌ | ✔️ |
| `explode: true` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |
| `explode: false` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |

### URL Compound Parameter

Compound (struct-typed) `x-zserio-request-part` is unsupported across all components.

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| `x-zserio-request-part: <[parent.]*compound_member>` | ❌️ | ❌️ | ❌️ | ❌️ | ❌️ |

### Server URL base path

Each client takes the URL base path from `servers[0]`:

```yaml
servers:
- http://unused-host-information/path/to/my/api
```

The host/port comes from the request, but the path prefix is taken from this entry.

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| `servers` | ✔️ | ✔️ | ✔️ | ✔️ | ✔️ |

### Authentication schemes

OpenAPI's `securitySchemes` and `security` fields drive auth. Per-operation `security:` overrides the root-level one; `security: []` explicitly disables auth for an operation.

Supported schemes:

- **HTTP Basic** — credentials checked from `httpcl::Config::auth` / `HttpConfig.auth` / Python `HTTPConfig.basic_auth`. Throws if missing.
- **HTTP Bearer** — verifies an `Authorization: Bearer <token>` header is present. Throws if missing.
- **API key (cookie/header/query)** — applies the configured `api-key` to the matching location, or verifies the user has provided it directly.
- **OAuth2 client credentials** — clients automatically acquire, cache, refresh access tokens from the configured token endpoint. Two token-endpoint authentication methods are supported: `rfc6749-client-secret-basic` (default) and `rfc5849-oauth1-signature` (HMAC-SHA256). See [`docs/http-settings.md`](docs/http-settings.md) for full configuration.

If you don't want to put credentials in [`HTTP_SETTINGS_FILE`](docs/http-settings.md), pass `httpcl::Config` (C++) / `HTTPConfig` (Python) / `HttpConfig` (Java) directly to the client constructor.

| Feature | C++ Client | Python Client | Java Client | OAServer | zswag.gen |
|---|---|---|---|---|---|
| HTTP Basic / HTTP Bearer / Cookie API-Key / Header API-Key / Query API-Key | ✔️ | ✔️ | ✔️ | ✔️(\*\*) | ✔️ |
| `OAuth2[clientCredentials]` | ✔️ | ✔️ | ✔️ | ✔️(\*\*) | ✔️ |
| `OpenID Connect` `OAuth2[authorizationCode]` `OAuth2[implicit]` `OAuth2[password]` | ❌️ | ❌️ | ❌️ | ✔️(\*\*) | ❌️ |

**(\*\*)** OAServer's actual support depends on your WSGI server (Apache/Nginx/...) wrapping the Flask app.
