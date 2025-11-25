# Next Steps for Java Client Implementation

**Status**: Desktop implementation ✅ Complete | Android implementation ⏳ Pending

This document outlines the remaining work to complete the Java client implementation for zswag.

---

## 🔧 Phase 1: Desktop Refinements (Estimated: 3-5 days)

### 1.1 Parameter Encoding Fixes
**Priority**: High
**Status**: In Progress
**Estimated Time**: 1-2 days

Current issues discovered during integration testing:

- **Header Parameters**: Fix header parameter passing (e.g., X-Ponent for power endpoint)
  - Location: `libs/jzswag-desktop/src/main/java/com/ndsev/zswag/desktop/DesktopOpenAPIClient.java`
  - Issue: Headers from HttpSettings not being merged with operation parameters

- **String Array Encoding**: Fix concat() getting 'foo,bar' instead of 'foobar'
  - Location: `libs/jzswag-desktop/src/main/java/com/ndsev/zswag/desktop/ParameterEncoder.java`
  - Issue: Array encoding format selection based on parameter specifications

- **Cookie Authentication**: Fix HTTP 401 errors for cookie-authenticated endpoints
  - Location: `libs/jzswag-desktop/src/main/java/com/ndsev/zswag/desktop/DesktopHttpClient.java`
  - Issue: Cookie headers not being properly set from HttpSettings

### 1.2 Unit Tests
**Priority**: High
**Estimated Time**: 2-3 days

Create comprehensive unit tests:

- **ParameterEncoder Tests**
  - All parameter styles (simple, label, matrix, form, etc.)
  - All formats (string, hex, base64, base64url, binary)
  - Array handling (explode true/false)
  - Edge cases (empty arrays, special characters, etc.)

- **OpenAPIParser Tests**
  - YAML and JSON parsing
  - Server URL extraction
  - Security scheme parsing
  - Operation extraction

- **DesktopHttpClient Tests**
  - Mock server integration
  - Authentication header injection
  - Timeout handling
  - SSL configuration

- **OAuth2Handler Tests**
  - Token acquisition
  - Token caching and expiry
  - Thread safety
  - Refresh flow

**Test Framework**: JUnit 5 + Mockito + AssertJ + MockWebServer (already configured)

### 1.3 Documentation Polish
**Priority**: Medium
**Estimated Time**: 1 day

- Complete Javadoc for all public APIs
- Add more usage examples to GETTING_STARTED_JAVA.md
- Create architecture diagram
- Add troubleshooting section

---

## 📱 Phase 2: Android Implementation (Estimated: 3-4 weeks)

### 2.1 Android Module Setup
**Priority**: High
**Estimated Time**: 1 week

- Create `libs/jzswag-android/` module
- Configure Android Gradle plugin (AGP 8.x)
- Set up Android SDK requirements (minSdk 24, targetSdk 34)
- Configure ProGuard/R8 rules for zserio reflection
- Set up Android test infrastructure (Robolectric + Espresso)

**Files to Create**:
```
libs/jzswag-android/
├── build.gradle
├── proguard-rules.pro
├── src/
│   ├── main/
│   │   ├── AndroidManifest.xml
│   │   └── java/com/ndsev/zswag/android/
│   │       ├── AndroidHttpClient.java         (OkHttp-based)
│   │       ├── AndroidOpenAPIClient.java      (reuse desktop logic)
│   │       ├── AndroidConfigurationLoader.java (SharedPreferences)
│   │       └── AndroidOAuth2Handler.java      (with token refresh)
│   └── test/
│       └── java/
└── README.md
```

### 2.2 Android HTTP Client (OkHttp)
**Priority**: High
**Estimated Time**: 3-4 days

Implement `AndroidHttpClient` using OkHttp 4.x:

- Connection pooling configuration
- Certificate pinning support (for security)
- Network security config integration
- Timeout configuration per request
- Interceptor for logging (debug builds only)
- HTTP/2 support
- Response caching (optional)

**Dependencies**:
```gradle
implementation 'com.squareup.okhttp3:okhttp:4.12.0'
implementation 'com.squareup.okhttp3:logging-interceptor:4.12.0'
```

### 2.3 Android Configuration Management
**Priority**: Medium
**Estimated Time**: 2-3 days

Android-specific configuration handling:

- **SharedPreferences Integration**
  - Store non-sensitive HTTP settings
  - Per-host configuration
  - Preference change listeners

- **Android Keystore Integration**
  - Secure credential storage (Basic Auth, Bearer tokens)
  - Encrypted SharedPreferences for OAuth2 tokens
  - Biometric authentication support (optional)

- **Lifecycle-Aware Configuration**
  - ViewModel integration for configuration
  - LiveData/Flow for reactive updates

### 2.4 Kotlin Coroutines Support
**Priority**: Medium
**Estimated Time**: 2-3 days

Add Kotlin-friendly async APIs:

```kotlin
// Suspend function variants
interface IOpenAPIClient {
    suspend fun callMethodAsync(
        methodPath: String,
        parameters: Map<String, Any>,
        requestBody: ByteArray?
    ): ByteArray?
}

// Flow-based reactive APIs
fun observeConfiguration(): Flow<HttpSettings>
```

**Dependencies**:
```gradle
implementation 'org.jetbrains.kotlinx:kotlinx-coroutines-android:1.7.3'
```

### 2.5 Android Testing
**Priority**: High
**Estimated Time**: 3-4 days

- **Unit Tests** (Robolectric)
  - AndroidHttpClient with MockWebServer
  - SharedPreferences mocking
  - Keystore mocking

- **Instrumentation Tests** (Espresso)
  - Real HTTP requests
  - Network security config validation
  - Certificate pinning tests

- **Integration Tests**
  - Test against Calculator service
  - Authentication flow tests
  - Background task handling

---

## 🚗 Phase 3: Android Automotive Demo (Estimated: 1-2 weeks)

### 3.1 AAOS Application
**Priority**: Medium
**Estimated Time**: 1-2 weeks

Create `examples/jzswag-aaos/` demo application:

**Features**:
- Car services integration (sensors, HVAC, media)
- OpenAPI service communication demo
- Template-based UI (ListTemplate, GridTemplate, PaneTemplate)
- Voice interaction support
- Driver distraction optimization

**Files to Create**:
```
examples/jzswag-aaos/
├── build.gradle
├── src/main/
│   ├── AndroidManifest.xml
│   └── java/com/ndsev/zswag/aaos/
│       ├── CarServiceActivity.java
│       ├── ServiceCommunicationScreen.java
│       └── VoiceInteractionHandler.java
└── README.md
```

**Dependencies**:
```gradle
implementation 'androidx.car.app:app:1.4.0'
implementation 'androidx.car.app:app-automotive:1.4.0'
```

---

## 🔧 Phase 4: Additional Features (Optional)

### 4.1 Path Template Matching Enhancement
**Priority**: Low
**Current**: Basic operation ID lookup
**Enhancement**: Sophisticated path template matching with wildcards

### 4.2 OAuth2 Flow Extensions
**Priority**: Low
**Current**: Client credentials flow only
**Enhancement**:
- Authorization code flow
- PKCE support
- Refresh token handling
- Token revocation

### 4.3 Kotlin DSL Re-enablement
**Priority**: Low
**Issue**: Disabled due to Java 25 incompatibility
**Solution**:
- Downgrade to Java 17 or 21 for Kotlin compatibility
- Or wait for Kotlin to support Java 25
- Kotlin DSL provides fluent API for configuration

**Affected Files**:
```
libs/jzswag-api/src/main/kotlin-disabled/
├── HttpSettingsExtensions.kt
├── HttpRequestExtensions.kt
└── OpenAPIExtensions.kt
```

### 4.4 Reactive Programming Support
**Priority**: Low
**Platforms**: Desktop + Android
**Options**:
- RxJava 3 adapters
- Kotlin Flow integration (Android)
- CompletableFuture wrappers (Desktop)

### 4.5 Code Generation Tool
**Priority**: Low
**Goal**: Generate type-safe Java client code from OpenAPI specs
**Similar to**: Python's generated `Service.Client` classes
**Approach**: Gradle plugin using zserio + OpenAPI codegen

---

## 📊 Progress Tracking

### Completed ✅
- [x] Pure Java architecture design
- [x] jzswag-api module (shared interfaces)
- [x] jzswag-desktop module (complete implementation)
- [x] jzswag-cli example (command-line tool)
- [x] jzswag-test module (integration tests)
- [x] Core HTTP communication
- [x] OpenAPI 3.0 parsing
- [x] All parameter locations (path, query, header, body)
- [x] Basic parameter encoding
- [x] All authentication schemes (infrastructure)
- [x] OAuth2 client credentials flow (with caching)
- [x] Integration test script
- [x] Documentation (README files)

### In Progress 🔧
- [ ] Parameter encoding refinements (header params, cookies)
- [ ] Unit test coverage
- [ ] Integration test full pass (10/10 tests)

### Pending ⏳
- [ ] Android module implementation
- [ ] Android Automotive demo app
- [ ] Complete Javadoc coverage
- [ ] Advanced OAuth2 flows
- [ ] Kotlin DSL re-enablement

---

## 🎯 Recommended Next Actions

For the user to get the Java client to production-ready state:

### Short Term (This Week)
1. **Fix parameter encoding issues** (1-2 days)
   - Run integration tests to identify specific failures
   - Fix header parameter passing
   - Fix cookie authentication
   - Fix array encoding

2. **Add unit tests for ParameterEncoder** (1 day)
   - Cover all parameter styles and formats
   - Ensure encoding matches OpenAPI spec

### Medium Term (Next 2 Weeks)
3. **Complete Desktop unit tests** (2-3 days)
   - Mock server tests for HTTP client
   - Parser tests for OpenAPI spec loading
   - OAuth2 handler tests

4. **Begin Android module** (1 week)
   - Create module structure
   - Implement OkHttp-based HTTP client
   - Set up Android test infrastructure

### Long Term (Next 1-2 Months)
5. **Android implementation** (3-4 weeks)
   - Complete Android HTTP client
   - Configuration management
   - Coroutines support
   - Testing

6. **AAOS demo application** (1-2 weeks)
   - Full Android Automotive example
   - Car services integration
   - Documentation and guides

---

## 📚 Reference Documentation

**Existing Docs**:
- [GETTING_STARTED_JAVA.md](GETTING_STARTED_JAVA.md) - Java client usage guide
- [libs/jzswag-api/README.md](libs/jzswag-api/README.md) - API module documentation
- [libs/jzswag-desktop/README.md](libs/jzswag-desktop/README.md) - Desktop client guide
- [libs/jzswag-test/README.md](libs/jzswag-test/README.md) - Integration test documentation

**Related Resources**:
- [OpenAPI 3.0 Specification](https://spec.openapis.org/oas/v3.0.3)
- [zserio Language Reference](http://zserio.org/doc/ZserioLanguageOverview.html)
- [Android Automotive Documentation](https://source.android.com/devices/automotive)

---

**Last Updated**: 2025-11-25
**Java Client Version**: 1.11.0
**Status**: Desktop Complete ✅ | Android Pending ⏳
