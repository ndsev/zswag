# Getting Started with zswag Java Clients

## Quick Start

### Prerequisites

- Java 11 or higher
- Gradle 7.0+ (or use the wrapper once generated)
- zserio compiler (for generating service interfaces)

### Initialize Gradle Wrapper

```bash
gradle wrapper --gradle-version 8.5
```

### Build the Project

```bash
./gradlew build
```

### Run the Example CLI

```bash
# With a remote OpenAPI spec
./gradlew :examples:jzswag-cli:run \
  --args="https://petstore3.swagger.io/api/v3/openapi.json /pet/findByStatus status=available"

# With a local OpenAPI spec
./gradlew :examples:jzswag-cli:run \
  --args="path/to/your/openapi.yaml /your/endpoint param1=value1"
```

---

## What's Been Implemented

### ✅ Complete Components

1. **jzswag-api** - Shared API contracts
   - Platform-agnostic interfaces
   - Type-safe configuration builders
   - All OpenAPI parameter types
   - *(Kotlin DSL extensions temporarily disabled - Java 25 compatibility)*

2. **jzswag-desktop** - Desktop implementation
   - Java 11 HttpClient integration
   - OpenAPI 3.0 YAML/JSON parser
   - Complete parameter encoding (all styles and formats)
   - OAuth2 client credentials flow with caching
   - Configuration from YAML files and environment variables
   - zserio service integration

3. **jzswag-cli** - Example CLI application
   - Command-line interface for testing
   - Configuration loading
   - Response display (text and binary)

4. **jzswag-test** - Integration tests
   - 10 comprehensive test cases
   - Calculator service integration
   - All authentication schemes tested
   - Automated test script
   - Successfully validates HTTP communication

---

## Project Structure

```
zswag/
├── libs/
│   ├── jzswag-api/              # ✅ Shared interfaces
│   │   ├── src/main/java/       # Java interfaces and types
│   │   └── src/main/kotlin-disabled/  # Kotlin DSL (Java 25 compat issue)
│   │
│   ├── jzswag-desktop/          # ✅ Desktop implementation
│   │   ├── src/main/java/       # Implementation classes
│   │   └── src/test/java/       # ⏳ Unit tests (TODO)
│   │
│   ├── jzswag-test/             # ✅ Integration tests
│   │   ├── build.gradle         # zserio code generation
│   │   ├── test-java-client.bash  # Automated test script
│   │   └── src/main/java/
│   │       ├── calculator/      # Generated zserio classes
│   │       └── com/ndsev/zswag/test/
│   │           └── CalculatorTestClient.java
│   │
│   └── jzswag-android/          # ⏳ Android implementation (TODO)
│       ├── src/main/java/
│       └── src/main/kotlin/
│
├── examples/
│   ├── jzswag-cli/              # ✅ Desktop CLI example
│   ├── jzswag-aaos/             # ⏳ Android Automotive app (TODO)
│   └── integration-tests/       # ⏳ Integration tests (TODO)
│
├── build.gradle                 # Root build configuration
├── settings.gradle              # Multi-module project setup
└── JAVA_CLIENT_STATUS.md        # Detailed implementation status
```

---

## Usage Examples

### 1. Basic HTTP Client

```java
import com.ndsev.zswag.api.*;
import com.ndsev.zswag.desktop.*;
import java.time.Duration;

// Create HTTP settings
HttpSettings settings = HttpSettings.builder()
    .timeout(Duration.ofSeconds(60))
    .header("User-Agent", "MyApp/1.0")
    .sslStrict(true)
    .build();

// Create HTTP client
IHttpClient httpClient = new DesktopHttpClient(settings);

// Make a request
HttpRequest request = HttpRequest.builder()
    .method("GET")
    .url("https://api.example.com/data")
    .build();

HttpResponse response = httpClient.execute(request);
System.out.println("Status: " + response.getStatusCode());
```

### 2. OpenAPI Client

```java
import com.ndsev.zswag.desktop.*;
import java.util.*;

// Create OpenAPI client from spec
IOpenAPIClient client = new DesktopOpenAPIClient(
    "https://api.example.com/openapi.yaml",
    httpClient
);

// Call an API method
Map<String, Object> params = new HashMap<>();
params.put("userId", 123);
params.put("fields", Arrays.asList("name", "email"));

byte[] response = client.callMethod("/users/{userId}", params, null);
```

### 3. Configuration from File

Create `http-settings.yaml`:

```yaml
timeout: 30
sslStrict: true

headers:
  User-Agent: MyApp/1.0
  Accept: application/json

queryParameters:
  api_version: v2

bearerToken: your-bearer-token-here

apiKeys:
  X-API-Key: your-api-key-here
```

Load it:

```java
import com.ndsev.zswag.desktop.ConfigurationLoader;

// Load from HTTP_SETTINGS_FILE environment variable or defaults
HttpSettings settings = ConfigurationLoader.loadSettings();

// Or load from specific file
HttpSettings settings = ConfigurationLoader.loadFromFile("http-settings.yaml");
```

### 4. OAuth2 Authentication

```java
import com.ndsev.zswag.desktop.OAuth2Handler;

// Create OAuth2 handler
OAuth2Handler oauth2 = new OAuth2Handler(
    "https://auth.example.com/oauth/token",  // Token endpoint
    "client-id",                              // Client ID
    "client-secret",                          // Client Secret
    "read write",                             // Scopes (optional)
    httpClient
);

// Get access token (cached and auto-refreshed)
String accessToken = oauth2.getAccessToken();

// Use in HTTP settings
HttpSettings settings = HttpSettings.builder()
    .bearerToken(accessToken)
    .build();
```

### 5. zserio Service Client

```java
import com.ndsev.zswag.desktop.ZswagServiceClient;

// Create zserio service client
ZswagServiceClient serviceClient = ZswagServiceClient.create(
    "com.example.Calculator",                 // Service identifier
    "https://api.example.com/openapi.yaml",   // OpenAPI spec
    settings                                  // HTTP settings
);

// Serialize request
byte[] requestData = SerializeUtil.serializeToBytes(calcRequest);

// Call method
byte[] responseData = serviceClient.callMethod("calculate", requestData, context);

// Deserialize response
CalcResponse response = SerializeUtil.deserializeFromBytes(
    CalcResponse.class,
    responseData
);
```

### 6. Kotlin DSL

```kotlin
import com.ndsev.zswag.api.*
import java.time.Duration

// Build settings with DSL
val settings = httpSettings {
    timeout = Duration.ofSeconds(60)
    header("User-Agent", "MyApp/1.0")
    bearerToken = "your-token"
    sslStrict = true
}

// Make API call with DSL
val response = client.call("/users/{id}") {
    param("id", userId)
    param("include", listOf("profile", "settings"))
}

// Async calls (platform implementations can provide suspend functions)
val response = client.callAsync("/users/{id}") {
    param("id", userId)
}
```

---

## Environment Variables

Configure the client via environment variables:

- `HTTP_SETTINGS_FILE` - Path to YAML configuration file
- `HTTP_TIMEOUT` - Request timeout in seconds (e.g., `60`)
- `HTTP_SSL_STRICT` - Enable strict SSL verification (`0` or `1`)
- `HTTP_BEARER_TOKEN` - Bearer token for authentication

Example:

```bash
export HTTP_SETTINGS_FILE=/path/to/http-settings.yaml
export HTTP_TIMEOUT=60
export HTTP_SSL_STRICT=1
export HTTP_BEARER_TOKEN=your-token-here

./gradlew :examples:jzswag-cli:run --args="spec.yaml /endpoint"
```

---

## Next Steps

### For Desktop Development

1. **Add Unit Tests**
   ```bash
   # Create tests in libs/jzswag-desktop/src/test/java/
   ./gradlew :libs:jzswag-desktop:test
   ```

2. **Test with Your OpenAPI Spec**
   ```bash
   ./gradlew :examples:jzswag-cli:run \
     --args="your-spec.yaml /your/endpoint param=value"
   ```

3. **Integrate with Your zserio Services**
   - Generate Java classes from your .zs files
   - Use ZswagServiceClient to connect to your services

### For Android Automotive Development

The Android implementation is **not yet started**. To begin:

1. **Create Android Module**
   ```bash
   mkdir -p libs/jzswag-android/src/main/{java,kotlin,res}
   # Copy build.gradle template (Android Library plugin)
   ```

2. **Implement Android Components**
   - OkHttp-based HTTP client
   - SharedPreferences configuration
   - Android Keystore integration
   - Coroutines support

3. **Create AAOS Demo App**
   - Set up Android Automotive project
   - Implement service integration
   - Add car services integration

**Estimated Timeline**: 3-4 weeks for Android implementation

---

## Testing Your Implementation

### With curl (for comparison)

```bash
# Test an OpenAPI endpoint with curl
curl -X GET "https://api.example.com/users/123?fields=name,email" \
  -H "Authorization: Bearer your-token"

# Then test with jzswag-cli
./gradlew :examples:jzswag-cli:run \
  --args="https://api.example.com/openapi.yaml /users/{userId} userId=123 fields=name,email"
```

### With Mock Server

Use libraries like `mockwebserver` (OkHttp) or `wiremock` to create test servers:

```java
// In your tests
MockWebServer server = new MockWebServer();
server.enqueue(new MockResponse()
    .setBody("{\"status\": \"ok\"}")
    .setResponseCode(200));
server.start();

IHttpClient client = new DesktopHttpClient(settings);
// Test against server.url("/")
```

---

## Troubleshooting

### Build Issues

1. **Missing Gradle Wrapper**
   ```bash
   gradle wrapper --gradle-version 8.5
   ```

2. **Java Version Issues**
   ```bash
   # Check Java version
   java -version  # Should be 11 or higher

   # Set JAVA_HOME if needed
   export JAVA_HOME=/path/to/jdk-11
   ```

3. **Dependency Resolution**
   ```bash
   # Clear Gradle cache and rebuild
   ./gradlew clean build --refresh-dependencies
   ```

### Runtime Issues

1. **SSL Certificate Errors**
   ```bash
   # Disable strict SSL (not recommended for production)
   export HTTP_SSL_STRICT=0
   ```

2. **Connection Timeouts**
   ```bash
   # Increase timeout
   export HTTP_TIMEOUT=120
   ```

3. **OpenAPI Spec Not Found**
   ```bash
   # Use absolute path or full URL
   ./gradlew :examples:jzswag-cli:run \
     --args="file:///absolute/path/to/spec.yaml /endpoint"
   ```

---

## Architecture Comparison

### vs C++ Implementation

| Feature | C++ (libs/zswagcl) | Java Desktop (libs/jzswag-desktop) |
|---------|-------------------|-----------------------------------|
| HTTP Client | cpp-httplib | Java 11 HttpClient |
| OpenAPI Parser | yaml-cpp | SnakeYAML |
| OAuth2 | Custom implementation | Custom implementation |
| Token Caching | Yes | Yes (thread-safe) |
| Config Files | YAML | YAML + Environment variables |
| Keychain | OS-specific | Java Keystore (TODO) |
| Binary Size | ~5-10MB | ~1-2MB (pure Java) |
| Dependencies | OpenSSL, yaml-cpp, etc. | SnakeYAML, Gson only |

### Key Differences

- **No JNI** - Pure Java implementation, no native code
- **Platform-specific optimizations** - Desktop uses Java 11 HttpClient, Android will use OkHttp
- **Idiomatic APIs** - Java builders and Kotlin DSL
- **Simplified dependencies** - Fewer external libraries

---

## Contributing

To contribute to the Java client implementation:

1. Follow the existing code style (see `.editorconfig`)
2. Add unit tests for new features
3. Update documentation (README files and Javadoc)
4. Test on both Java 11 and Java 17
5. Ensure Kotlin DSL extensions work properly

---

## Support

For questions and issues:
- Check [JAVA_CLIENT_STATUS.md](JAVA_CLIENT_STATUS.md) for implementation status
- Review the C++ implementation in `libs/zswagcl/` for reference behavior
- See existing tests in `libs/zswag/test/` for integration patterns

---

**Last Updated**: 2025-11-25
**Status**: Desktop Complete (Phase 2), Android Pending (Phase 3)
