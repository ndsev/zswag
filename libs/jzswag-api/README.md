# jzswag-api

Shared Java/Kotlin API interfaces for zswag OpenAPI clients.

## Overview

This module defines the common API contract that both Desktop and Android implementations of the zswag client adhere to. It provides:

- **Interfaces**: `IHttpClient`, `IOpenAPIClient`, `IZswagServiceClient`
- **Configuration**: `HttpSettings`, `OpenAPIParameter`, `SecurityScheme`
- **Types**: Parameter locations, styles, formats, and security scheme types
- **Kotlin DSL**: Extension functions for idiomatic Kotlin usage

## Usage

### Java

```java
// Build HTTP settings
HttpSettings settings = HttpSettings.builder()
    .header("X-API-Key", "your-key")
    .timeout(Duration.ofSeconds(60))
    .bearerToken("your-token")
    .build();

// Make HTTP request
HttpRequest request = HttpRequest.builder()
    .method("GET")
    .url("https://api.example.com/users")
    .headers(settings.getHeaders())
    .build();
```

### Kotlin

```kotlin
// Build HTTP settings with DSL
val settings = httpSettings {
    header("X-API-Key", "your-key")
    timeout = Duration.ofSeconds(60)
    bearerToken = "your-token"
}

// Make HTTP request with DSL
val request = httpRequest {
    method = "GET"
    url = "https://api.example.com/users"
    headers(settings.headers)
}

// Call OpenAPI method with DSL
val response = client.call("/users/{id}") {
    param("id", userId)
    param("include", listOf("profile", "settings"))
}
```

## Implementations

- **jzswag-desktop**: Desktop implementation using Java 11 HttpClient
- **jzswag-android**: Android implementation using OkHttp and Android-specific APIs

## Requirements

- Java 11+
- zserio Java runtime 2.16.1+
- Kotlin 1.9.22+ (for Kotlin extensions)

## License

Same as the parent zswag project.
