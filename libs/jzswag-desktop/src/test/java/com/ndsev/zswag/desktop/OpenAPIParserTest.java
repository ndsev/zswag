package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.Set;

import static org.assertj.core.api.Assertions.*;

/**
 * Unit tests for OpenAPIParser.
 * Tests YAML and JSON parsing, server URL extraction, security schemes, and operation parsing.
 */
class OpenAPIParserTest {

    @TempDir
    Path tempDir;

    private Path yamlSpecPath;
    private OpenAPIParser parser;

    @BeforeEach
    void setUp() throws IOException {
        // Copy test spec to temp directory
        yamlSpecPath = tempDir.resolve("openapi.yaml");
        String yamlContent = new String(getClass().getResourceAsStream("/test-openapi.yaml").readAllBytes());
        Files.writeString(yamlSpecPath, yamlContent);
        parser = new OpenAPIParser(yamlSpecPath.toString());
    }

    @Nested
    @DisplayName("Server URL Tests")
    class ServerUrlTests {

        @Test
        @DisplayName("Should parse server URLs")
        void parseServerUrls() {
            List<String> servers = parser.getServers();
            assertThat(servers).hasSize(2);
            assertThat(servers.get(0)).isEqualTo("https://api.example.com/v1");
            assertThat(servers.get(1)).isEqualTo("https://backup.example.com/v1");
        }

        @Test
        @DisplayName("Should return empty list when no servers defined")
        void noServers() throws IOException {
            String noServerSpec = "openapi: \"3.0.0\"\n" +
                    "info:\n" +
                    "  title: No Server API\n" +
                    "  version: \"1.0.0\"\n" +
                    "paths: {}\n";
            Path path = tempDir.resolve("no-server.yaml");
            Files.writeString(path, noServerSpec);
            OpenAPIParser p = new OpenAPIParser(path.toString());
            assertThat(p.getServers()).isEmpty();
        }
    }

    @Nested
    @DisplayName("Security Scheme Tests")
    class SecuritySchemeTests {

        @Test
        @DisplayName("Should parse Bearer auth scheme")
        void parseBearerAuth() {
            Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
            assertThat(schemes).containsKey("BearerAuth");

            SecurityScheme bearer = schemes.get("BearerAuth");
            assertThat(bearer.getType()).isEqualTo(SecuritySchemeType.HTTP);
            assertThat(bearer.getScheme()).isEqualTo("bearer");
        }

        @Test
        @DisplayName("Should parse Basic auth scheme")
        void parseBasicAuth() {
            Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
            assertThat(schemes).containsKey("BasicAuth");

            SecurityScheme basic = schemes.get("BasicAuth");
            assertThat(basic.getType()).isEqualTo(SecuritySchemeType.HTTP);
            assertThat(basic.getScheme()).isEqualTo("basic");
        }

        @Test
        @DisplayName("Should parse API Key in header scheme")
        void parseApiKeyHeader() {
            Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
            assertThat(schemes).containsKey("ApiKeyAuth");

            SecurityScheme apiKey = schemes.get("ApiKeyAuth");
            assertThat(apiKey.getType()).isEqualTo(SecuritySchemeType.API_KEY);
            assertThat(apiKey.getApiKeyLocation()).isEqualTo(ParameterLocation.HEADER);
            assertThat(apiKey.getApiKeyName()).isEqualTo("X-API-Key");
        }

        @Test
        @DisplayName("Should parse API Key in query scheme")
        void parseApiKeyQuery() {
            Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
            assertThat(schemes).containsKey("QueryKeyAuth");

            SecurityScheme apiKey = schemes.get("QueryKeyAuth");
            assertThat(apiKey.getType()).isEqualTo(SecuritySchemeType.API_KEY);
            assertThat(apiKey.getApiKeyLocation()).isEqualTo(ParameterLocation.QUERY);
            assertThat(apiKey.getApiKeyName()).isEqualTo("api_key");
        }

        @Test
        @DisplayName("Should parse API Key in cookie scheme")
        void parseApiKeyCookie() {
            Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
            assertThat(schemes).containsKey("CookieAuth");

            SecurityScheme apiKey = schemes.get("CookieAuth");
            assertThat(apiKey.getType()).isEqualTo(SecuritySchemeType.API_KEY);
            assertThat(apiKey.getApiKeyLocation()).isEqualTo(ParameterLocation.COOKIE);
            assertThat(apiKey.getApiKeyName()).isEqualTo("session_id");
        }

        @Test
        @DisplayName("Should parse OAuth2 scheme")
        void parseOAuth2() {
            Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
            assertThat(schemes).containsKey("OAuth2Auth");

            SecurityScheme oauth = schemes.get("OAuth2Auth");
            assertThat(oauth.getType()).isEqualTo(SecuritySchemeType.OAUTH2);
        }

        @Test
        @DisplayName("Should parse all security schemes")
        void parseAllSchemes() {
            Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
            assertThat(schemes).hasSize(6);
            assertThat(schemes.keySet()).containsExactlyInAnyOrder(
                    "BearerAuth", "BasicAuth", "ApiKeyAuth", "QueryKeyAuth", "CookieAuth", "OAuth2Auth"
            );
        }
    }

    @Nested
    @DisplayName("Operation Parsing Tests")
    class OperationTests {

        @Test
        @DisplayName("Should find method by operation ID")
        void findByOperationId() {
            OpenAPIParser.MethodInfo method = parser.getMethod("getUser");
            assertThat(method).isNotNull();
            assertThat(method.getHttpMethod()).isEqualTo("GET");
            assertThat(method.getPathTemplate()).isEqualTo("/users/{userId}");
        }

        @Test
        @DisplayName("Should parse path parameters")
        void parsePathParameters() {
            OpenAPIParser.MethodInfo method = parser.getMethod("getUser");
            assertThat(method).isNotNull();

            List<OpenAPIParameter> params = method.getParameters();
            assertThat(params).anyMatch(p ->
                    p.getName().equals("userId") &&
                    p.getLocation() == ParameterLocation.PATH &&
                    p.isRequired()
            );
        }

        @Test
        @DisplayName("Should parse header parameters")
        void parseHeaderParameters() {
            OpenAPIParser.MethodInfo method = parser.getMethod("getUser");
            assertThat(method).isNotNull();

            List<OpenAPIParameter> params = method.getParameters();
            assertThat(params).anyMatch(p ->
                    p.getName().equals("X-Request-ID") &&
                    p.getLocation() == ParameterLocation.HEADER &&
                    !p.isRequired()
            );
        }

        @Test
        @DisplayName("Should parse query parameters with explode")
        void parseQueryParametersWithExplode() {
            OpenAPIParser.MethodInfo method = parser.getMethod("listItems");
            assertThat(method).isNotNull();

            List<OpenAPIParameter> params = method.getParameters();
            assertThat(params).anyMatch(p ->
                    p.getName().equals("ids") &&
                    p.getLocation() == ParameterLocation.QUERY &&
                    p.isExplode() &&
                    p.getFormat() == ParameterFormat.HEX
            );
        }

        @Test
        @DisplayName("Should parse operation security requirements")
        void parseOperationSecurity() {
            OpenAPIParser.MethodInfo getUser = parser.getMethod("getUser");
            assertThat(getUser.getSecurityRequirements()).containsExactly("BearerAuth");

            OpenAPIParser.MethodInfo createItem = parser.getMethod("createItem");
            assertThat(createItem.getSecurityRequirements()).containsExactly("BasicAuth");

            OpenAPIParser.MethodInfo listItems = parser.getMethod("listItems");
            assertThat(listItems.getSecurityRequirements()).containsExactly("ApiKeyAuth");
        }

        @Test
        @DisplayName("Should handle empty security (public endpoint)")
        void parseEmptySecurity() {
            OpenAPIParser.MethodInfo method = parser.getMethod("publicEndpoint");
            assertThat(method).isNotNull();
            assertThat(method.getSecurityRequirements()).isEmpty();
        }

        @Test
        @DisplayName("Should parse POST method")
        void parsePostMethod() {
            OpenAPIParser.MethodInfo method = parser.getMethod("createItem");
            assertThat(method).isNotNull();
            assertThat(method.getHttpMethod()).isEqualTo("POST");
        }

        @Test
        @DisplayName("Should return null for unknown operation")
        void unknownOperation() {
            assertThat(parser.getMethod("nonExistent")).isNull();
        }
    }

    @Nested
    @DisplayName("JSON Parsing Tests")
    class JsonParsingTests {

        @Test
        @DisplayName("Should parse JSON format spec")
        void parseJsonSpec() throws IOException {
            String jsonSpec = "{\n" +
                    "  \"openapi\": \"3.0.0\",\n" +
                    "  \"info\": {\"title\": \"JSON API\", \"version\": \"1.0.0\"},\n" +
                    "  \"servers\": [{\"url\": \"https://json.example.com\"}],\n" +
                    "  \"paths\": {\n" +
                    "    \"/test\": {\n" +
                    "      \"get\": {\n" +
                    "        \"operationId\": \"testOp\",\n" +
                    "        \"responses\": {\"200\": {\"description\": \"OK\"}}\n" +
                    "      }\n" +
                    "    }\n" +
                    "  }\n" +
                    "}";
            Path jsonPath = tempDir.resolve("spec.json");
            Files.writeString(jsonPath, jsonSpec);

            OpenAPIParser jsonParser = new OpenAPIParser(jsonPath.toString());
            assertThat(jsonParser.getServers()).containsExactly("https://json.example.com");
            assertThat(jsonParser.getMethod("testOp")).isNotNull();
        }
    }

    @Nested
    @DisplayName("Edge Cases")
    class EdgeCaseTests {

        @Test
        @DisplayName("Should handle spec without security schemes")
        void noSecuritySchemes() throws IOException {
            String spec = "openapi: \"3.0.0\"\n" +
                    "info:\n" +
                    "  title: No Security API\n" +
                    "  version: \"1.0.0\"\n" +
                    "paths:\n" +
                    "  /test:\n" +
                    "    get:\n" +
                    "      operationId: testOp\n" +
                    "      responses:\n" +
                    "        '200':\n" +
                    "          description: OK\n";
            Path path = tempDir.resolve("no-security.yaml");
            Files.writeString(path, spec);
            OpenAPIParser p = new OpenAPIParser(path.toString());
            assertThat(p.getSecuritySchemes()).isEmpty();
        }

        @Test
        @DisplayName("Should handle operation without parameters")
        void noParameters() throws IOException {
            String spec = "openapi: \"3.0.0\"\n" +
                    "info:\n" +
                    "  title: Simple API\n" +
                    "  version: \"1.0.0\"\n" +
                    "paths:\n" +
                    "  /test:\n" +
                    "    get:\n" +
                    "      operationId: simpleOp\n" +
                    "      responses:\n" +
                    "        '200':\n" +
                    "          description: OK\n";
            Path path = tempDir.resolve("simple.yaml");
            Files.writeString(path, spec);
            OpenAPIParser p = new OpenAPIParser(path.toString());
            OpenAPIParser.MethodInfo method = p.getMethod("simpleOp");
            assertThat(method).isNotNull();
            assertThat(method.getParameters()).isEmpty();
        }

        @Test
        @DisplayName("Should throw for invalid spec file")
        void invalidSpecFile() {
            assertThatThrownBy(() -> new OpenAPIParser("/nonexistent/path.yaml"))
                    .isInstanceOf(IOException.class);
        }

        @Test
        @DisplayName("Should handle relative server URL")
        void relativeServerUrl() throws IOException {
            String spec = "openapi: \"3.0.0\"\n" +
                    "info:\n" +
                    "  title: Relative Server API\n" +
                    "  version: \"1.0.0\"\n" +
                    "servers:\n" +
                    "  - url: /api/v1\n" +
                    "paths: {}\n";
            Path path = tempDir.resolve("relative.yaml");
            Files.writeString(path, spec);
            OpenAPIParser p = new OpenAPIParser(path.toString());
            assertThat(p.getServers()).containsExactly("/api/v1");
        }
    }
}
