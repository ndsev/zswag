# jzswag-android

Android port of the zswag client. Built on OkHttp + the platform Keystore. Pulls in `jzswag-shared` for the platform-agnostic core (OpenAPI dispatch, parameter encoding, OAuth2 flow, YAML loader); only the HTTP transport, keychain, and logging are Android-specific.

## Role in the project

- Implements zserio's `zserio.runtime.service.ServiceClientInterface` via `ZswagClient`, so a zserio-Java-generated `XClient` accepts an instance as its transport — same idiom as the JVM port and Python's `services.MyService.Client(OAClient(url))`.
- Performs the same `x-zserio-request-part` decomposition the JVM client does (logic lives in `jzswag-shared`).
- Handles the same authentication schemes: HTTP Basic, HTTP Bearer, API key (header/query/cookie), OAuth2 client credentials with both token-endpoint auth methods.
- Loads the same `HTTP_SETTINGS_FILE` YAML format as the C++/Python/JVM clients.
- Stores credentials in the platform Keystore: an AES-256-GCM key generated in the secure enclave (TEE / StrongBox where available) encrypts per-credential entries that live in a private `SharedPreferences` file.

## Public API

- `ZswagClient(Context, String url[, HttpSettings persistent[, HttpConfig adhoc]])` — main entry point. The `Context` parameter is the only public-API difference from the JVM port; needed so `AndroidKeychain` can reach `SharedPreferences`.
- `AndroidHttpClient` — `IHttpClient` implementation on top of OkHttp.
- `AndroidKeychain` — `IKeychain` implementation on top of the Android Keystore + AES-GCM-encrypted SharedPreferences. Apps store credentials via `AndroidKeychain.store(service, user, secret)`; zswag itself only ever loads.
- `AndroidLogging.init()` — symmetric to the JVM `JzswagLogging.init()`. On Android, log filtering is logcat-driven (`setprop log.tag.<TAG>`); the call is a near-noop.

## Build trade-off (read this)

This module uses the plain `java-library` Gradle plugin instead of `com.android.library`. The reason: Google currently ships only x86_64-Linux `aapt2` binaries, and on aarch64 Linux build hosts the AGP-driven build fails with "AAPT2 daemon startup failed" on resource-free library modules. There is no community aarch64 build of `aapt2` either.

Effect:
- Output is a JAR rather than an AAR. Android consumers can still depend on it (just less idiomatically).
- AndroidX dependencies are unavailable (java-library can't consume AAR deps). `AndroidKeychain` therefore uses raw Android Keystore APIs + AES manually instead of `EncryptedSharedPreferences`.
- `android.*` references compile against the Robolectric `android-all` stub jar; the real framework is provided at runtime by the consuming app.

On an x86_64 build host (or with Rosetta on Apple Silicon Macs), the module can be flipped back to `com.android.library` for AAR output with no source changes — the existing `local.properties` + Android SDK install setup are already there.

## Dependencies

- `jzswag-shared` (transitively pulls `jzswag-api`, zserio-runtime, SnakeYAML, Gson, slf4j-api).
- OkHttp 4.12.0 — HTTP transport.
- `uk.uuid.slf4j:slf4j-android` 2.0.9-0 — SLF4J binding routing through `android.util.Log`. Marked `runtimeOnly` so it doesn't appear on the test classpath (where `android.util.Log` isn't available).

## Testing

```bash
./gradlew :libs:jzswag-android:test
```

Line coverage ≥60%, but with caveats: AndroidHttpClient has full coverage via OkHttp's `MockWebServer` (it's pure Java around OkHttp, no `android.*` refs). AndroidKeychain's encrypt/decrypt round trip and AndroidLogging's log-level routing path can't run on the aarch64 sandbox (Robolectric pulls Conscrypt which has no aarch64-linux native, and `androidx.test:monitor` is AAR-only) — those paths need a device or an x86_64 host with Robolectric.
