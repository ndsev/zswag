# OAuth2 Mock Server

A standalone OAuth2 server for testing OAuth 1.0 signature and HTTP Basic Auth token authentication.

## Features

- **Token Endpoint** (`POST /oauth2/token`)
  - Supports OAuth 1.0 HMAC-SHA256 signature authentication (RFC 5849)
  - Supports HTTP Basic authentication (RFC 6749)
  - Supports public clients (no authentication)
  - Issues Bearer tokens for protected API access

- **Protected API Endpoint** (`GET /api/protected`)
  - Requires valid Bearer token
  - Requires `read` scope

- **Public API Endpoint** (`GET /api/public`)
  - No authentication required

- **OpenAPI Specification** (`GET /openapi.json`)
  - Describes all endpoints and OAuth2 security scheme
  - Compatible with `zswag.OAClient`

## Usage

### Start the Server

```bash
python -m zswag.test.oauth2_mock --host 127.0.0.1 --port 8080
```

The server logs authentication method used for each request.

### Test Clients

Two test clients are pre-configured:

1. **test-client**
   - Client ID: `test-client`
   - Secret: `test-secret`
   - Scopes: `read`, `write`, `admin`

2. **test-access-key-id**
   - Client ID: `test-access-key-id`
   - Secret: `test-access-key-secret`
   - Scopes: `read`, `write`

## Testing with curl

### OAuth 1.0 Signature Authentication

This is complex to do manually with curl. Use the zswag client instead (see below).

### HTTP Basic Authentication

```bash
# Get token using Basic Auth
TOKEN=$(curl -s -X POST http://localhost:8080/oauth2/token \
  -u test-client:test-secret \
  -d "grant_type=client_credentials" \
  | jq -r .access_token)

# Call protected endpoint
curl -H "Authorization: Bearer $TOKEN" \
  http://localhost:8080/api/protected
```

## Testing with zswag OAClient

### Configure HTTP Settings

Create `~/.http-settings.yaml`:

```yaml
# OAuth 1.0 Signature Auth
- scope: http://localhost:8080/*
  oauth2:
    clientId: test-client
    clientSecret: test-secret
    tokenUrl: http://localhost:8080/oauth2/token
    tokenEndpointAuth:
      method: rfc5849-oauth1-signature
      nonceLength: 16

# Or use HTTP Basic Auth (comment out the above, use this instead)
# - scope: http://localhost:8080/*
#   oauth2:
#     clientId: test-client
#     clientSecret: test-secret
#     tokenUrl: http://localhost:8080/oauth2/token
#     tokenEndpointAuth:
#       method: rfc6749-client-secret-basic
```

### Python Client Example

```python
from zswag import OAClient

# Create client (will auto-discover OpenAPI spec)
client = OAClient("http://localhost:8080/openapi.json")

# HTTP settings are loaded automatically from ~/.http-settings.yaml
# The client will:
# 1. Request token using configured auth method
# 2. Receive Bearer token
# 3. Call protected endpoint with Bearer token

# Your zserio service calls work normally
# result = my_service.MyMethod(request)
```

## Server Logs

The server logs show which authentication method was used:

```
[17:30:15] [oauth2-mock] [TOKEN] Request from 127.0.0.1
[17:30:15] [oauth2-mock] [TOKEN] grant_type=client_credentials, scope=['read'], audience=
[17:30:15] [oauth2-mock] [TOKEN] Attempting OAuth 1.0 signature authentication
[17:30:15] [oauth2-mock] [TOKEN] ✓ Authenticated via OAuth 1.0 HMAC-SHA256 Signature: client_id=test-client
[17:30:15] [oauth2-mock] [TOKEN] ✓ Issued token for client=test-client, scopes=['read']
[17:30:15] [oauth2-mock] [API] Bearer token valid for client=test-client, scopes=['read']
```

## Implementation Details

- **OAuth 1.0 Signature**: Full RFC 5849 implementation with HMAC-SHA256
- **Token Storage**: In-memory (tokens expire after 3600 seconds)
- **Scope Validation**: Enforced on both token issuance and API access
- **Logging**: All authentication attempts and methods logged

## Use Cases

1. **Testing OAuth 1.0 signature support** before deploying to production
2. **Verifying zswag client configuration** without external dependencies
3. **Integration testing** for applications using OAuth2 client credentials flow
4. **Comparing authentication methods** (OAuth 1.0 vs HTTP Basic)
