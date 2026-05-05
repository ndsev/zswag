# Next Steps for Java Client Implementation

**Status**: Desktop implementation ✅ Complete (incl. unit tests) | Integration tests ⏳ Re-verify | Android ⏳ Pending

This document outlines the remaining work to complete the Java client implementation for zswag.

---

## 🔁 Session Handoff (2026-05-05)

Use this section to resume work on a different machine/session.

### Branch state
- Branch: `jzswag`, pushed to `origin/jzswag`
- Rebased cleanly onto `origin/main` (currently `994a94a`); 4 commits ahead:
  1. `7a7e7a2` First pure JAVA zswag impl (wip)
  2. `fb3ab06` Fixes after code review
  3. `3f3d7e8` More fixes
  4. `bff53a0` test: Add unit tests for jzswag-desktop client
  5. `706fbad` build: Add placeholder build.gradle for jzswag-android and jzswag-aaos
- No PR opened yet.

### What just landed
- **Unit tests (~1.8k LOC, 28 test classes/nested groups, all passing)** for `ParameterEncoder`, `OAuth2Handler`, `OpenAPIParser`, `DesktopHttpClient` (JUnit 5 + Mockito + AssertJ + MockWebServer).
- **`build.gradle` stubs** for `libs/jzswag-android` and `examples/jzswag-aaos`. Without them, `./gradlew clean` (or a fresh checkout) failed with "Configuring project … without an existing directory is not allowed" because git does not track empty dirs.

### Verified build commands (Java 25.0.1, Gradle 9.2.1, macOS arm64)
```bash
./gradlew clean build                    # full multi-module build, green
./gradlew :libs:jzswag-desktop:test      # 28 test groups, all PASSED
```

### Immediate next actions (pick up here)

#### 1. Re-run the Calculator integration test loop  ← start here
NEXT_STEPS.md previously flagged 3 known bugs (X-Ponent header param, string-array `concat()` encoding, cookie auth 401s). The unit tests we just landed cover the encoder thoroughly and pass — so it's worth re-running the integration test to see whether those bugs are actually still present or whether the "More fixes" / "Fixes after code review" commits silently fixed them.

```bash
# 1. Build the Python wheel (needed by the test harness)
mkdir -p build && cd build
cmake -DZSWAG_BUILD_WHEELS=ON -DZSWAG_ENABLE_TESTING=OFF -DZSWAG_KEYCHAIN_SUPPORT=OFF ..
cmake --build .
cd ..

# 2. Install it
pip install -r requirements.txt
pip install build/bin/wheel/*.whl

# 3. Run the integration test
./libs/jzswag-test/test-java-client.bash
```
Look for failures in: `power` (X-Ponent header), `concat` (string array → expected "foobar", was "foo,bar"), `floatMul`/`identity` (cookie auth 401). Expected outcome is one of:
- All 10 tests pass → mark Phase 1 done, open the PR.
- Some still fail → fix in `DesktopOpenAPIClient.java` / `ParameterEncoder.java` / `DesktopHttpClient.java` per the diagnostic notes in `libs/jzswag-test/README.md`.

#### 2. Open a draft PR
Once integration tests are reproducibly run (passing or with a known set of remaining failures):
```bash
gh pr create --base main --head jzswag --draft \
  --title "Pure Java (jzswag-desktop) client" \
  --body-file <(...)   # summary of components, test status, link to NEXT_STEPS.md
```

#### 3. Then Phase 2 (Android module)
See section below. ~3-4 weeks of work. Do not start before #1 + #2.

### Open watch-items
- `./gradlew clean` will delete the empty `src/main/java/...` chains under the new placeholder modules but the build still succeeds because the stub `build.gradle` is tracked. If you ever add real source, commit a `.gitkeep` or actual file in the source dir.
- Gradle 9.2.1 emits "incompatible with Gradle 10" deprecation warnings. Run `./gradlew build --warning-mode all` once before bumping Gradle to see what needs fixing.
- `libs/jzswag-api/src/main/kotlin-disabled/` exists because Kotlin doesn't yet support Java 25 (per the README). Re-enable when Kotlin catches up — see "Kotlin DSL Re-enablement" below.

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
- [x] **Unit test coverage for jzswag-desktop** (ParameterEncoder, OAuth2Handler, OpenAPIParser, DesktopHttpClient — JUnit 5/Mockito/AssertJ/MockWebServer, all green)
- [x] **Rebase onto `origin/main`** (1.11.1 release, codecov/sonar, security fixes — clean, no conflicts)
- [x] **Placeholder build.gradle for jzswag-android and jzswag-aaos** (so `./gradlew clean` and fresh checkouts no longer break)

### In Progress 🔧
- [ ] Re-run integration tests to confirm whether the 3 known parameter-encoding/auth bugs are still present
- [ ] Open draft PR against `main`

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

**Last Updated**: 2026-05-05
**Java Client Version**: 1.11.0 (rebased on top of 1.11.1 main)
**Status**: Desktop Complete + Unit-tested ✅ | Integration tests pending re-run 🔧 | Android Pending ⏳
