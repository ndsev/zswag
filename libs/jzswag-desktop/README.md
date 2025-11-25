# jzswag-desktop

Pure Java desktop implementation of the zswag OpenAPI client using Java 11 HttpClient.

## Features

- ✅ **Java 11 HttpClient** - Modern, built-in HTTP client
- ✅ **OpenAPI 3.0 Support** - YAML/JSON specification parsing
- ✅ **Parameter Encoding** - All OpenAPI parameter styles (simple, label, matrix, form, etc.)
- ✅ **Authentication** - Basic, Bearer, API Key support
- ✅ **OAuth2** - Client credentials flow with automatic token refresh
- ✅ **Configuration** - YAML files and environment variables
- ✅ **zserio Integration** - Seamless integration with zserio services
- ✅ **Thread-safe** - Concurrent request handling

## Usage

### Basic Example

```java
import com.ndsev.zswag.api.*;
import com.ndsev.zswag.desktop.*;

// Create HTTP settings
HttpSettings settings = HttpSettings.builder()
    .header("X-API-Key", "your-key")
    .timeout(Duration.ofSeconds(60))
    .build();

// Create HTTP client
IHttpClient httpClient = new DesktopHttpClient(settings);

// Create OpenAPI client
IOpenAPIClient client = new DesktopOpenAPIClient(
    "https://api.example.com/openapi.yaml",
    httpClient
);

// Call an API method
Map<String, Object> params = new HashMap<>();
params.put("userId", 123);
params.put("include", Arrays.asList("profile", "settings"));

byte[] response = client.callMethod("/users/{userId}", params, null);
```

### zserio Service Integration

```java
import com.ndsev.zswag.desktop.ZswagServiceClient;

// Create zserio service client
ZswagServiceClient serviceClient = ZswagServiceClient.create(
    "com.example.MyService",
    "https://api.example.com/openapi.yaml",
    settings
);

// Use with zserio-generated service
byte[] request = SerializeUtil.serializeToBytes(myRequest);
byte[] response = serviceClient.callMethod("myMethod", request, context);
MyResponse result = SerializeUtil.deserializeFromBytes(MyResponse.class, response);
```

### Configuration File

Create an `http-settings.yaml`:

```yaml
headers:
  User-Agent: MyApp/1.0
  X-Custom-Header: value

queryParameters:
  api_version: v1

timeout: 30
sslStrict: true
proxyUrl: http://proxy.example.com:8080

basicAuth:
  username: user
  password: pass

bearerToken: your-bearer-token

apiKeys:
  X-API-Key: your-api-key
```

Load it:

```java
// From environment variable HTTP_SETTINGS_FILE
HttpSettings settings = ConfigurationLoader.loadSettings();

// Or from specific file
HttpSettings settings = ConfigurationLoader.loadFromFile("http-settings.yaml");
```

### OAuth2 Client Credentials

```java
OAuth2Handler oauth2 = new OAuth2Handler(
    "https://auth.example.com/token",
    "client-id",
    "client-secret",
    "read write",
    httpClient
);

String token = oauth2.getAccessToken(); // Cached and auto-refreshed

HttpSettings settings = HttpSettings.builder()
    .bearerToken(token)
    .build();
```

## Environment Variables

- `HTTP_SETTINGS_FILE` - Path to configuration YAML file
- `HTTP_TIMEOUT` - Request timeout in seconds
- `HTTP_SSL_STRICT` - Enable strict SSL verification (0/1)
- `HTTP_BEARER_TOKEN` - Bearer token for authentication

## Requirements

- Java 11+
- zserio Java runtime 2.16.1+

## Dependencies

- SnakeYAML - YAML parsing
- Gson - JSON handling
- SLF4J - Logging interface
- Logback - Logging implementation (runtime)

## Testing

Run tests with:
```bash
cd libs/jzswag-desktop
gradle test
```

## License

Same as the parent zswag project.
