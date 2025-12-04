#include <catch2/catch_all.hpp>

#include "httpcl/http-settings.hpp"

#include <filesystem>
#include <fstream>
#include <thread>
#include <cstdlib>
#include <string>

namespace fs = std::filesystem;

// Cross-platform environment variable helpers
#ifdef _WIN32
inline void test_setenv(const char* name, const char* value) {
    _putenv_s(name, value);
}
inline void test_unsetenv(const char* name) {
    std::string var = std::string(name) + "=";
    _putenv(var.c_str());
}
#else
inline void test_setenv(const char* name, const char* value) {
    setenv(name, value, 1);
}
inline void test_unsetenv(const char* name) {
    unsetenv(name);
}
#endif

// Helper fixture class for managing temporary test files
class SettingsTestFixture {
public:
    SettingsTestFixture() {
        // Create unique temp directory for this test
        tempDir = fs::temp_directory_path() / ("httpcl_test_" + generateRandomString());
        fs::create_directories(tempDir);
        tempFile = tempDir / "http-settings.yml";
    }

    ~SettingsTestFixture() {
        // Clean up environment variable
        test_unsetenv("HTTP_SETTINGS_FILE");

        // Clean up temporary files and directory
        try {
            if (fs::exists(tempDir)) {
                fs::remove_all(tempDir);
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    void setEnvironmentVariable() {
        test_setenv("HTTP_SETTINGS_FILE", tempFile.string().c_str());
    }

    void writeFile(const std::string& content) {
        std::ofstream os(tempFile);
        os << content;
        os.close();
    }

    std::string readFile() {
        std::ifstream is(tempFile);
        std::string content((std::istreambuf_iterator<char>(is)),
                           std::istreambuf_iterator<char>());
        return content;
    }

    fs::path getTempFile() const {
        return tempFile;
    }

private:
    static std::string generateRandomString() {
        static const char* chars = "0123456789abcdef";
        std::string result(12, '.');
        for (auto& c : result) {
            c = chars[rand() % 16];
        }
        return result;
    }

    fs::path tempDir;
    fs::path tempFile;
};

// =============================================================================
// YAML Encoding Tests (configToNode)
// =============================================================================

TEST_CASE("EncodeScopeConfiguration", "[http-settings][yaml][encode]") {
    httpcl::Config config;
    config.scope = "https://api.example.com";

    auto yaml = config.toYaml();

    REQUIRE(yaml.find("scope: https://api.example.com") != std::string::npos);
    REQUIRE(yaml.find("url:") == std::string::npos);
}

TEST_CASE("EncodeUrlPatternConfiguration", "[http-settings][yaml][encode]") {
    httpcl::Config config;
    config.urlPatternString = "^https://.*\\.example\\.com.*$";
    config.urlPattern = std::regex(config.urlPatternString);

    auto yaml = config.toYaml();

    REQUIRE(yaml.find("url: ^https://.*\\.example\\.com.*$") != std::string::npos);
    REQUIRE(yaml.find("scope:") == std::string::npos);
}

TEST_CASE("EncodeCompleteConfigWithAllFields", "[http-settings][yaml][encode]") {
    httpcl::Config config;
    config.scope = "https://api.example.com";
    config.cookies["session"] = "abc123";
    config.headers.insert({"X-Custom-Header", "value1"});
    config.headers.insert({"X-Another-Header", "value2"});
    config.query.insert({"param1", "value1"});
    config.query.insert({"param2", "value2"});
    config.apiKey = "secret-api-key";

    auto yaml = config.toYaml();

    REQUIRE(yaml.find("scope: https://api.example.com") != std::string::npos);
    REQUIRE(yaml.find("session: abc123") != std::string::npos);
    REQUIRE(yaml.find("X-Custom-Header: value1") != std::string::npos);
    REQUIRE(yaml.find("X-Another-Header: value2") != std::string::npos);
    REQUIRE(yaml.find("param1: value1") != std::string::npos);
    REQUIRE(yaml.find("param2: value2") != std::string::npos);
    REQUIRE(yaml.find("api-key: secret-api-key") != std::string::npos);
}

TEST_CASE("EncodeOAuth2WithKeychainAndPartialFields", "[http-settings][yaml][encode]") {
    httpcl::Config config;
    config.scope = "https://oauth.example.com";

    httpcl::Config::OAuth2 oauth2;
    oauth2.clientId = "client123";
    oauth2.clientSecretKeychain = "keychain-service-id";
    oauth2.tokenUrlOverride = "https://auth.example.com/token";
    oauth2.audience = "https://api.example.com";
    config.oauth2 = oauth2;

    auto yaml = config.toYaml();

    REQUIRE(yaml.find("oauth2:") != std::string::npos);
    REQUIRE(yaml.find("clientId: client123") != std::string::npos);
    REQUIRE(yaml.find("clientSecretKeychain: keychain-service-id") != std::string::npos);
    REQUIRE(yaml.find("tokenUrl: https://auth.example.com/token") != std::string::npos);
    REQUIRE(yaml.find("audience: https://api.example.com") != std::string::npos);
    REQUIRE(yaml.find("clientSecret:") == std::string::npos);
}

TEST_CASE("EncodeBasicAuthWithKeychain", "[http-settings][yaml][encode]") {
    httpcl::Config config;
    config.scope = "https://api.example.com";

    httpcl::Config::BasicAuthentication auth;
    auth.user = "testuser";
    auth.keychain = "keychain-service-123";
    config.auth = auth;

    auto yaml = config.toYaml();

    REQUIRE(yaml.find("basic-auth:") != std::string::npos);
    REQUIRE(yaml.find("user: testuser") != std::string::npos);
    REQUIRE(yaml.find("keychain: keychain-service-123") != std::string::npos);
    REQUIRE(yaml.find("password:") == std::string::npos);
}

// =============================================================================
// YAML Decoding Tests (configFromNode)
// =============================================================================

TEST_CASE("DecodeMinimalConfiguration", "[http-settings][yaml][decode]") {
    std::string yaml = R"(
url: ^https://api\.example\.com.*$
)";

    httpcl::Config config(yaml);

    REQUIRE(config.urlPatternString == "^https://api\\.example\\.com.*$");
    REQUIRE(!config.scope.has_value());
    REQUIRE(config.cookies.empty());
    REQUIRE(config.headers.empty());
    REQUIRE(config.query.empty());
    REQUIRE(!config.auth.has_value());
    REQUIRE(!config.proxy.has_value());
    REQUIRE(!config.apiKey.has_value());
    REQUIRE(!config.oauth2.has_value());
}

TEST_CASE("DecodeScopeWithWildcardConversion", "[http-settings][yaml][decode]") {
    std::string yaml = R"(
scope: https://*.example.com
)";

    httpcl::Config config(yaml);

    REQUIRE(config.scope.has_value());
    REQUIRE(*config.scope == "https://*.example.com");
    // Verify wildcard was converted to regex
    REQUIRE(config.urlPatternString.find(".*") != std::string::npos);
    REQUIRE(config.urlPatternString.find("^") == 0);
    REQUIRE(config.urlPatternString.find(".*$") != std::string::npos);
}

TEST_CASE("DecodeDefaultScopeWhenMissing", "[http-settings][yaml][decode]") {
    std::string yaml = R"(
cookies:
  session: test123
)";

    httpcl::Config config(yaml);

    REQUIRE(config.scope.has_value());
    REQUIRE(*config.scope == "*");
    REQUIRE(config.cookies["session"] == "test123");
}

TEST_CASE("DecodeCompleteConfiguration", "[http-settings][yaml][decode]") {
    std::string yaml = R"(
scope: https://api.example.com
cookies:
  session: abc123
  tracking: xyz789
headers:
  X-Custom-Header: value1
  Authorization: Bearer token
query:
  api_key: key123
  version: v1
api-key: secret-api-key
basic-auth:
  user: testuser
  password: testpass
proxy:
  host: proxy.example.com
  port: 8080
  user: proxyuser
  password: proxypass
oauth2:
  clientId: client123
  clientSecret: secret456
  tokenUrl: https://auth.example.com/token
  refreshUrl: https://auth.example.com/refresh
  audience: https://api.example.com
  scope:
    - read
    - write
)";

    httpcl::Config config(yaml);

    REQUIRE(config.scope.has_value());
    REQUIRE(*config.scope == "https://api.example.com");
    REQUIRE(config.cookies.size() == 2);
    REQUIRE(config.cookies["session"] == "abc123");
    REQUIRE(config.cookies["tracking"] == "xyz789");
    REQUIRE(config.headers.size() == 2);
    REQUIRE(config.query.size() == 2);
    REQUIRE(config.apiKey.has_value());
    REQUIRE(*config.apiKey == "secret-api-key");

    REQUIRE(config.auth.has_value());
    REQUIRE(config.auth->user == "testuser");
    REQUIRE(config.auth->password == "testpass");

    REQUIRE(config.proxy.has_value());
    REQUIRE(config.proxy->host == "proxy.example.com");
    REQUIRE(config.proxy->port == 8080);
    REQUIRE(config.proxy->user == "proxyuser");
    REQUIRE(config.proxy->password == "proxypass");

    REQUIRE(config.oauth2.has_value());
    REQUIRE(config.oauth2->clientId == "client123");
    REQUIRE(config.oauth2->clientSecret == "secret456");
    REQUIRE(config.oauth2->tokenUrlOverride == "https://auth.example.com/token");
    REQUIRE(config.oauth2->refreshUrlOverride == "https://auth.example.com/refresh");
    REQUIRE(config.oauth2->audience == "https://api.example.com");
    REQUIRE(config.oauth2->scopesOverride.size() == 2);
    REQUIRE(config.oauth2->scopesOverride[0] == "read");
    REQUIRE(config.oauth2->scopesOverride[1] == "write");
}

TEST_CASE("DecodeProxyWithoutAuthentication", "[http-settings][yaml][decode]") {
    std::string yaml = R"(
scope: https://api.example.com
proxy:
  host: proxy.example.com
  port: 8080
)";

    httpcl::Config config(yaml);

    REQUIRE(config.proxy.has_value());
    REQUIRE(config.proxy->host == "proxy.example.com");
    REQUIRE(config.proxy->port == 8080);
    REQUIRE(config.proxy->user.empty());
    REQUIRE(config.proxy->password.empty());
    REQUIRE(config.proxy->keychain.empty());
}

// =============================================================================
// YAML Round-trip Test
// =============================================================================

TEST_CASE("RoundTripEncodeDecode", "[http-settings][yaml][roundtrip]") {
    httpcl::Config original;
    original.scope = "https://api.example.com";
    original.cookies["session"] = "test123";
    original.headers.insert({"X-Header", "value"});
    original.query.insert({"param", "value"});
    original.apiKey = "secret-key";

    httpcl::Config::BasicAuthentication auth;
    auth.user = "user123";
    auth.password = "pass456";
    original.auth = auth;

    // Encode to YAML
    auto yaml = original.toYaml();

    // Decode from YAML
    httpcl::Config decoded(yaml);

    // Verify all fields match
    REQUIRE(decoded.scope.has_value());
    REQUIRE(*decoded.scope == *original.scope);
    REQUIRE(decoded.cookies == original.cookies);
    REQUIRE(decoded.apiKey.has_value());
    REQUIRE(*decoded.apiKey == *original.apiKey);
    REQUIRE(decoded.auth.has_value());
    REQUIRE(decoded.auth->user == original.auth->user);
    REQUIRE(decoded.auth->password == original.auth->password);
}

// =============================================================================
// Settings Load Tests
// =============================================================================

TEST_CASE("LoadSettingsFromValidFile", "[http-settings][settings][load]") {
    SettingsTestFixture fixture;

    std::string yaml = R"(
http-settings:
  - scope: https://api1.example.com
    api-key: key1
  - scope: https://api2.example.com
    api-key: key2
)";

    fixture.writeFile(yaml);
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    REQUIRE(settings.settings.size() == 2);
    REQUIRE(settings.settings[0].scope.has_value());
    REQUIRE(*settings.settings[0].scope == "https://api1.example.com");
    REQUIRE(settings.settings[0].apiKey.has_value());
    REQUIRE(*settings.settings[0].apiKey == "key1");
    REQUIRE(settings.settings[1].scope.has_value());
    REQUIRE(*settings.settings[1].scope == "https://api2.example.com");
    REQUIRE(settings.settings[1].apiKey.has_value());
    REQUIRE(*settings.settings[1].apiKey == "key2");
}

TEST_CASE("LoadSettingsOldFormatCompatibility", "[http-settings][settings][load]") {
    SettingsTestFixture fixture;

    // Old format: root is an array, not a map with "http-settings" key
    std::string yaml = R"(
- scope: https://api.example.com
  api-key: oldformat-key
- url: ^https://.*\.example\.com$
  api-key: oldformat-key2
)";

    fixture.writeFile(yaml);
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    REQUIRE(settings.settings.size() == 2);
    REQUIRE(settings.settings[0].scope.has_value());
    REQUIRE(*settings.settings[0].scope == "https://api.example.com");
    REQUIRE(settings.settings[0].apiKey.has_value());
    REQUIRE(*settings.settings[0].apiKey == "oldformat-key");
}

TEST_CASE("LoadSettingsWithMissingFile", "[http-settings][settings][load]") {
    SettingsTestFixture fixture;

    // Set environment variable to non-existent file
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    // Should not throw, should just have empty settings
    REQUIRE(settings.settings.empty());
}

TEST_CASE("LoadSettingsWithEmptyEnvironmentVariable", "[http-settings][settings][load]") {
    // Explicitly set empty environment variable
    test_setenv("HTTP_SETTINGS_FILE", "");

    httpcl::Settings settings;

    // Should not throw, should just have empty settings
    REQUIRE(settings.settings.empty());

    test_unsetenv("HTTP_SETTINGS_FILE");
}

TEST_CASE("LoadSettingsWithInvalidYAML", "[http-settings][settings][load]") {
    SettingsTestFixture fixture;

    std::string invalidYaml = R"(
http-settings:
  - scope: https://api.example.com
    api-key: key1
  - this is not valid YAML: [unclosed bracket
)";

    fixture.writeFile(invalidYaml);
    fixture.setEnvironmentVariable();

    // Should not throw, should handle error gracefully
    httpcl::Settings settings;
    REQUIRE(settings.settings.empty());
}

// =============================================================================
// Settings Store Tests
// =============================================================================

TEST_CASE("StoreSettingsToFile", "[http-settings][settings][store]") {
    SettingsTestFixture fixture;
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    // Add some configurations
    httpcl::Config config1;
    config1.scope = "https://api1.example.com";
    config1.apiKey = "stored-key1";
    settings.settings.push_back(config1);

    httpcl::Config config2;
    config2.scope = "https://api2.example.com";
    config2.apiKey = "stored-key2";
    settings.settings.push_back(config2);

    settings.store();

    // Verify file was written
    REQUIRE(fs::exists(fixture.getTempFile()));

    // Verify content
    auto content = fixture.readFile();
    REQUIRE(content.find("api1.example.com") != std::string::npos);
    REQUIRE(content.find("stored-key1") != std::string::npos);
    REQUIRE(content.find("api2.example.com") != std::string::npos);
    REQUIRE(content.find("stored-key2") != std::string::npos);
}

TEST_CASE("StoreWithoutEnvironmentVariable", "[http-settings][settings][store]") {
    // Make sure environment variable is not set
    test_unsetenv("HTTP_SETTINGS_FILE");

    httpcl::Settings settings;

    httpcl::Config config;
    config.scope = "https://api.example.com";
    config.apiKey = "test-key";
    settings.settings.push_back(config);

    // Should not throw, should just log warning
    REQUIRE_NOTHROW(settings.store());
}

// =============================================================================
// URL Pattern Matching Tests
// =============================================================================

TEST_CASE("UrlMatchingSinglePattern", "[http-settings][url-matching]") {
    SettingsTestFixture fixture;

    std::string yaml = R"(
http-settings:
  - scope: https://api.example.com
    api-key: test-key
    headers:
      X-Custom: value
)";

    fixture.writeFile(yaml);
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    // Matching URL
    auto config = settings["https://api.example.com/endpoint"];
    REQUIRE(config.apiKey.has_value());
    REQUIRE(*config.apiKey == "test-key");
    REQUIRE(config.headers.size() == 1);

    // Non-matching URL
    auto config2 = settings["https://other.example.com/endpoint"];
    REQUIRE(!config2.apiKey.has_value());
    REQUIRE(config2.headers.empty());
}

TEST_CASE("UrlMatchingMultiplePatternsWithMerge", "[http-settings][url-matching]") {
    SettingsTestFixture fixture;

    std::string yaml = R"(
http-settings:
  - scope: https://*.example.com
    headers:
      X-Header1: value1
  - scope: https://api.example.com
    headers:
      X-Header2: value2
    api-key: specific-key
)";

    fixture.writeFile(yaml);
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    // URL matching both patterns
    auto config = settings["https://api.example.com/endpoint"];
    REQUIRE(config.apiKey.has_value());
    REQUIRE(*config.apiKey == "specific-key");
    REQUIRE(config.headers.size() == 2);

    // URL matching only wildcard pattern
    auto config2 = settings["https://other.example.com/endpoint"];
    REQUIRE(!config2.apiKey.has_value());
    REQUIRE(config2.headers.size() == 1);
}

TEST_CASE("UrlMatchingNoMatch", "[http-settings][url-matching]") {
    SettingsTestFixture fixture;

    std::string yaml = R"(
http-settings:
  - scope: https://api.example.com
    api-key: test-key
)";

    fixture.writeFile(yaml);
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    auto config = settings["https://completely-different.com/endpoint"];

    REQUIRE(!config.apiKey.has_value());
    REQUIRE(config.cookies.empty());
    REQUIRE(config.headers.empty());
    REQUIRE(config.query.empty());
}

TEST_CASE("UrlMatchingWithComplexRegexCharacters", "[http-settings][url-matching]") {
    SettingsTestFixture fixture;

    std::string yaml = R"(
http-settings:
  - scope: https://api.example.com/v1/*
    api-key: v1-key
  - scope: https://api.example.com/v2/*
    api-key: v2-key
)";

    fixture.writeFile(yaml);
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    // Test v1 endpoint
    auto config1 = settings["https://api.example.com/v1/users"];
    REQUIRE(config1.apiKey.has_value());
    REQUIRE(*config1.apiKey == "v1-key");

    // Test v2 endpoint
    auto config2 = settings["https://api.example.com/v2/users"];
    REQUIRE(config2.apiKey.has_value());
    REQUIRE(*config2.apiKey == "v2-key");

    // Test non-matching endpoint
    auto config3 = settings["https://api.example.com/v3/users"];
    REQUIRE(!config3.apiKey.has_value());
}

TEST_CASE("UrlMatchingReloadOnTimestampUpdate", "[http-settings][url-matching][reload]") {
    SettingsTestFixture fixture;

    std::string yaml1 = R"(
http-settings:
  - scope: https://api.example.com
    api-key: original-key
)";

    fixture.writeFile(yaml1);
    fixture.setEnvironmentVariable();

    httpcl::Settings settings;

    // Initial load
    auto config1 = settings["https://api.example.com/endpoint"];
    REQUIRE(config1.apiKey.has_value());
    REQUIRE(*config1.apiKey == "original-key");

    // Update file with new content
    std::string yaml2 = R"(
http-settings:
  - scope: https://api.example.com
    api-key: updated-key
)";

    fixture.writeFile(yaml2);

    // Update timestamp to trigger reload
    httpcl::Settings::updateTimestamp(std::chrono::steady_clock::now());

    // Small delay to ensure timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Access again - should reload
    auto config2 = settings["https://api.example.com/endpoint"];
    REQUIRE(config2.apiKey.has_value());
    REQUIRE(*config2.apiKey == "updated-key");
}
