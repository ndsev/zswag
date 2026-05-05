# HTTP Settings File

The Python (`OAClient` / `HttpLibHttpClient`), C++, and Java clients all read a YAML file pointed to by the `HTTP_SETTINGS_FILE` environment variable. The format is identical across all three clients — the same file works for all of them.

If `HTTP_SETTINGS_FILE` is unset or empty, no persistent settings are applied.

## Schema

```yaml
http-settings:
  - scope: "*"          # URL match pattern (glob), e.g. https://*.example.com/*
                        # Use 'url:' instead for raw regex.
    basic-auth:         # Basic auth credentials for matching requests.
      user: johndoe
      keychain: keychain-service-string   # OR
      password: cleartext-password
    proxy:              # HTTP proxy.
      host: localhost
      port: 8888
      user: test                          # optional
      keychain: ...                       # OR
      password: cleartext-password
    cookies:            # Additional cookies for matching requests.
      key: value
    headers:            # Additional headers.
      X-Trace: enabled
    query:              # Additional query parameters.
      api_version: v2
    api-key: value      # API key — auto-routed to header/query/cookie based on the
                        # OpenAPI scheme's 'in:' (see Authentication Schemes section).
    oauth2:
      clientId: my-client-id              # REQUIRED
      clientSecretKeychain: kc-string     # RECOMMENDED — load from keychain
      clientSecret: cleartext-secret      # OR cleartext (discouraged)
      tokenUrl: https://issuer/oauth/token
      refreshUrl: https://issuer/oauth/token  # optional; defaults to tokenUrl
      audience: https://api.example.com/  # optional
      scope: ["api.read", "api.write"]    # optional override of per-operation scopes
      useForSpecFetch: true               # optional, default true
      tokenEndpointAuth:
        method: rfc6749-client-secret-basic   # OR rfc5849-oauth1-signature
        nonceLength: 16                       # only for rfc5849, range 8..64
```

A multi-scope file simply has multiple list entries; for a given request URL, **all matching scopes are merged** in declaration order, with later scopes overriding scalar fields. Multi-valued fields (`headers`, `query`, `cookies`) are unioned.

For `proxy` configs, `user` is optional; if `user` is set, then `password` or `keychain` is required.

## Scope matching

`scope:` is a shell-style glob with `*` as the only wildcard, matched against the full request URL after request building. Examples:

- `"*"` — matches all requests.
- `"https://*.foo.com/*"` — matches `https://api.foo.com/data` (the dot before `foo` is literal — `https://foo.com/` does NOT match).
- `"http://localhost:5555/*"` — matches local dev servers on a specific port.

To match by raw regex instead, use `url:` in place of `scope:`:

```yaml
http-settings:
  - url: "^https?://(api|admin)\\.example\\.com/.*$"
    headers: ...
```

## OAuth2

Only the `clientCredentials` flow is supported across all zswag clients. Other flows (`authorizationCode`, `implicit`, `password`) and OpenID Connect cause the spec parser to reject the security scheme.

### Field requirements

| Field | Required? | Notes |
|---|---|---|
| `clientId` | Always | OAuth2 client identifier. |
| `tokenUrl` | When `useForSpecFetch: true` (default) | If `false`, the URL falls back to the spec's `flows.clientCredentials.tokenUrl`. |
| `clientSecret` / `clientSecretKeychain` | For confidential clients | Omit both for public clients (`client_id` goes in the request body). |
| `refreshUrl` | Optional | Defaults to spec value, then to `tokenUrl`. |
| `scope` | Optional | Defaults to per-operation scopes from the OpenAPI spec. |
| `audience` | Provider-specific | Some IdPs require it. |
| `useForSpecFetch` | Optional | Default `true`. Set `false` if the OpenAPI spec endpoint is publicly readable. |
| `tokenEndpointAuth` | Optional | Default `rfc6749-client-secret-basic`. |

### Precedence rules

When both `http-settings.yaml` and the OpenAPI spec specify a value:

1. **`tokenUrl`** — `http-settings.yaml` overrides the spec's `flows.clientCredentials.tokenUrl`.
2. **`refreshUrl`** — `http-settings.yaml` overrides the spec's `flows.clientCredentials.refreshUrl`.
3. **`scope`** — `http-settings.yaml` overrides the per-operation `security` scopes.

### Token endpoint authentication methods

Two authentication methods for the request **to the token endpoint** itself:

**`rfc6749-client-secret-basic` (default)** — RFC 6749 §2.3.1: `client_id:client_secret` in the `Authorization: Basic` header. Works with most providers.

**`rfc5849-oauth1-signature`** — RFC 5849: OAuth 1.0 HMAC-SHA256 signature. The token request is signed using the client secret; the secret itself is never transmitted. `nonceLength` controls the random nonce length (8–64). Required by some providers that use OAuth 1.0 signature-based token authentication.

### Spec fetch protection

By default (`useForSpecFetch: true`), the OAuth2 token is acquired **before** fetching the OpenAPI specification, so a spec endpoint that itself requires authentication can be reached. Set `useForSpecFetch: false` if your spec is public — this defers token acquisition to the first API call, which is faster.

### Debugging OAuth2

```bash
export HTTP_LOG_LEVEL=debug   # OAuth2 flow (mint/cache/refresh/auth method)
export HTTP_LOG_LEVEL=trace   # adds request/response bodies, signatures
```

## Keychain integration

Storing cleartext secrets in `http-settings.yaml` works but is discouraged. Use the `keychain:` field instead and pre-load the secret with the platform's native tool. The keychain "package" is `lib.openapi.zserio.client` (this is hardcoded across all zswag clients so secrets stored by one are visible to the others).

| Platform | Tool | Example |
|---|---|---|
| Linux | [`secret-tool`](https://www.marian-dan.ro/blog/storing-secrets-using-secret-tool) | `secret-tool store --label='zswag dev' package lib.openapi.zserio.client service my-service user my-user` |
| macOS | [`add-generic-password`](https://www.netmeister.org/blog/keychain-passwords.html) | `security add-generic-password -s my-service -a my-user -w 'thepassword'` |
| Windows | [`cmdkey`](https://www.scriptinglibrary.com/languages/powershell/how-to-manage-secrets-and-passwords-with-credentialmanager-and-powershell/) | (Java client: not yet implemented — use cleartext for now.) |

## Environment variables

| Variable | Effect |
|---|---|
| `HTTP_SETTINGS_FILE` | Path to YAML file. Empty/unset disables persistent config entirely. |
| `HTTP_LOG_LEVEL` | Verbosity (`debug`, `trace`). |
| `HTTP_LOG_FILE` | Logfile path. C++/Python use rotating logs (`HTTP_LOG_FILE`, `-1`, `-2`); Java client doesn't yet wire log file routing — configure logback directly. |
| `HTTP_LOG_FILE_MAXSIZE` | Rotation size in bytes. Default 1 GB. C++/Python only. |
| `HTTP_TIMEOUT` | Request timeout (connect + transfer) in seconds. Default `60`. |
| `HTTP_SSL_STRICT` | Set to a non-empty value (`1`, `true`) for strict certificate validation. Default strict. |

To disable persistent settings programmatically (e.g. in tests), set the env var to empty:

```python
import os
os.environ['HTTP_SETTINGS_FILE'] = ''
```

```cpp
setenv("HTTP_SETTINGS_FILE", "", 1);
```

```java
// Java: pass HttpSettings.empty() explicitly to the client constructor.
```
