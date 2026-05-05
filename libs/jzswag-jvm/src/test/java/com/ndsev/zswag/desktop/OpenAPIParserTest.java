package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class OpenAPIParserTest {

    private static Path writeSpec(Path dir, String yaml) throws IOException {
        Path p = dir.resolve("api.yaml");
        Files.writeString(p, yaml);
        return p;
    }

    @Test
    void parsesFixtureWithServersSchemesAndOperations() throws IOException {
        OpenAPIParser parser = new OpenAPIParser(
                Path.of("src/test/resources/test-openapi.yaml").toAbsolutePath().toString());
        assertThat(parser.getServers()).contains("https://api.example.com/v1", "https://backup.example.com/v1");
        assertThat(parser.getSecuritySchemes()).containsOnlyKeys(
                "BearerAuth", "BasicAuth", "ApiKeyAuth", "QueryKeyAuth", "CookieAuth", "OAuth2Auth");

        SecurityScheme bearer = parser.getSecuritySchemes().get("BearerAuth");
        assertThat(bearer.getType()).isEqualTo(SecuritySchemeType.HTTP);
        assertThat(bearer.getScheme()).isEqualTo("bearer");

        SecurityScheme apik = parser.getSecuritySchemes().get("ApiKeyAuth");
        assertThat(apik.getType()).isEqualTo(SecuritySchemeType.API_KEY);
        assertThat(apik.getApiKeyLocation()).isEqualTo(ParameterLocation.HEADER);
        assertThat(apik.getApiKeyName()).isEqualTo("X-API-Key");

        SecurityScheme oa2 = parser.getSecuritySchemes().get("OAuth2Auth");
        assertThat(oa2.getType()).isEqualTo(SecuritySchemeType.OAUTH2);
        assertThat(oa2.getTokenUrl()).contains("https://auth.example.com/token");
        assertThat(oa2.getOAuth2Scopes()).containsOnlyKeys("read", "write");
    }

    @Test
    void parsesGlobalDefaultSecurity() throws IOException {
        OpenAPIParser parser = new OpenAPIParser(
                Path.of("src/test/resources/test-openapi.yaml").toAbsolutePath().toString());
        assertThat(parser.getDefaultSecurity()).isPresent();
        assertThat(parser.getDefaultSecurity().get()).hasSize(1);
        assertThat(parser.getDefaultSecurity().get().get(0).getSchemes()).containsOnlyKeys("BearerAuth");
    }

    @Test
    void parsesOperationParametersAndPath(@TempDir Path dir) throws IOException {
        OpenAPIParser parser = new OpenAPIParser(
                Path.of("src/test/resources/test-openapi.yaml").toAbsolutePath().toString());
        OpenAPIParser.MethodInfo getUser = parser.getMethod("getUser");
        assertThat(getUser).isNotNull();
        assertThat(getUser.getHttpMethod()).isEqualTo("GET");
        assertThat(getUser.getPathTemplate()).isEqualTo("/users/{userId}");
        assertThat(getUser.getParameters()).hasSize(2);
        OpenAPIParameter userIdParam = getUser.getParameters().get(0);
        assertThat(userIdParam.getName()).isEqualTo("userId");
        assertThat(userIdParam.getLocation()).isEqualTo(ParameterLocation.PATH);
        assertThat(userIdParam.isRequired()).isTrue();
        assertThat(getUser.hasZserioBody()).isFalse();
    }

    @Test
    void operationWithEmptySecurityListMeansExplicitlyNoAuth() throws IOException {
        OpenAPIParser parser = new OpenAPIParser(
                Path.of("src/test/resources/test-openapi.yaml").toAbsolutePath().toString());
        OpenAPIParser.MethodInfo pub = parser.getMethod("publicEndpoint");
        assertThat(pub.getSecurity()).isPresent();
        assertThat(pub.getSecurity().get()).isEmpty();
    }

    @Test
    void emptyYamlThrowsIOException(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, "");
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IOException.class);
    }

    @Test
    void duplicateKeysAreRejected(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /a: {get: {operationId: a, responses: {'200': {description: ok}}}}",
                "paths:",
                "  /b: {get: {operationId: b, responses: {'200': {description: ok}}}}"));
        // SafeConstructor with allowDuplicateKeys=false throws
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOfAny(RuntimeException.class, IOException.class);
    }

    @Test
    void rejectsOAuth2WithUnsupportedFlow(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "components:",
                "  securitySchemes:",
                "    OA2:",
                "      type: oauth2",
                "      flows:",
                "        authorizationCode:",
                "          authorizationUrl: https://x/authorize",
                "          tokenUrl: https://x/token",
                "          scopes: {}",
                "paths: {}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("clientCredentials");
    }

    @Test
    void rejectsOAuth2WithMissingFlows(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "components:",
                "  securitySchemes:",
                "    OA2:",
                "      type: oauth2",
                "paths: {}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("flows");
    }

    @Test
    void rejectsUnknownSecuritySchemeType(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "components:",
                "  securitySchemes:",
                "    Bogus:",
                "      type: lol-not-a-real-type",
                "paths: {}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void rejectsUnknownParameterLocation(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x:",
                "    get:",
                "      operationId: x",
                "      parameters:",
                "        - name: foo",
                "          in: bogus",
                "          schema: {type: string}",
                "      responses: {'200': {description: ok}}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void rejectsUnknownParameterStyle(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x:",
                "    get:",
                "      operationId: x",
                "      parameters:",
                "        - name: foo",
                "          in: query",
                "          style: bogusStyle",
                "          schema: {type: string}",
                "      responses: {'200': {description: ok}}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void invalidStyleForLocationCausesError(@TempDir Path dir) throws IOException {
        // matrix style on a query parameter is invalid
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x:",
                "    get:",
                "      operationId: x",
                "      parameters:",
                "        - name: foo",
                "          in: query",
                "          style: matrix",
                "          schema: {type: string}",
                "      responses: {'200': {description: ok}}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("path parameters");
    }

    @Test
    void simpleStyleOnQueryRejected(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x:",
                "    get:",
                "      operationId: x",
                "      parameters:",
                "        - name: foo",
                "          in: query",
                "          style: simple",
                "          schema: {type: string}",
                "      responses: {'200': {description: ok}}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void formStyleOnPathRejected(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x/{y}:",
                "    get:",
                "      operationId: x",
                "      parameters:",
                "        - name: y",
                "          in: path",
                "          style: form",
                "          required: true",
                "          schema: {type: string}",
                "      responses: {'200': {description: ok}}"));
        assertThatThrownBy(() -> new OpenAPIParser(p.toString()))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void parsesXZserioRequestPartAndFormat(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /a/{id}:",
                "    post:",
                "      operationId: doIt",
                "      parameters:",
                "        - name: id",
                "          in: path",
                "          required: true",
                "          schema: {type: string, format: hex}",
                "          x-zserio-request-part: base.id",
                "        - name: blob",
                "          in: query",
                "          schema: {type: string, format: byte}",
                "          x-zserio-request-part: '*'",
                "      requestBody:",
                "        content:",
                "          application/x-zserio-object: {}",
                "      responses: {'200': {description: ok}}"));
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        OpenAPIParser.MethodInfo m = parser.getMethod("doIt");
        assertThat(m.hasZserioBody()).isTrue();
        OpenAPIParameter idParam = m.getParameters().get(0);
        assertThat(idParam.getRequestPart()).contains("base.id");
        assertThat(idParam.getFormat()).isEqualTo(ParameterFormat.HEX);
        OpenAPIParameter blobParam = m.getParameters().get(1);
        // 'byte' is an alias for base64 per OpenAPI
        assertThat(blobParam.getFormat()).isEqualTo(ParameterFormat.BASE64);
        assertThat(blobParam.isWholeRequest()).isTrue();
    }

    @Test
    void unknownFormatFallsBackToString(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /a:",
                "    get:",
                "      operationId: a",
                "      parameters:",
                "        - name: q",
                "          in: query",
                "          schema: {type: string, format: weirdcustomfmt}",
                "      responses: {'200': {description: ok}}"));
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        assertThat(parser.getMethod("a").getParameters().get(0).getFormat())
                .isEqualTo(ParameterFormat.STRING);
    }

    @Test
    void operationsWithoutOperationIdGetSyntheticId(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x:",
                "    get:",
                "      responses: {'200': {description: ok}}"));
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        // operationId == method + path with non-alphanumeric replaced
        assertThat(parser.getMethods()).containsKey("GET_x");
    }

    @Test
    void patchOperationIsIgnored(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x:",
                "    patch:",
                "      operationId: doPatch",
                "      responses: {'200': {description: ok}}"));
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        assertThat(parser.getMethods()).isEmpty();
    }

    @Test
    void requestBodyWithoutZserioObjectIsLoggedNotThrown(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "paths:",
                "  /x:",
                "    post:",
                "      operationId: a",
                "      requestBody:",
                "        content:",
                "          application/json: {schema: {type: object}}",
                "      responses: {'200': {description: ok}}"));
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        assertThat(parser.getMethod("a").hasZserioBody()).isFalse();
    }

    @Test
    void securitySchemeWithoutTypeDefaultsToHttp(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "components:",
                "  securitySchemes:",
                "    OnlyName: {scheme: basic}",
                "paths: {}"));
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        assertThat(parser.getSecuritySchemes().get("OnlyName").getType()).isEqualTo(SecuritySchemeType.HTTP);
    }

    @Test
    void openIdConnectSchemeIsAcceptedButLogged(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, String.join("\n",
                "openapi: '3.0.0'",
                "info: {title: t, version: '1'}",
                "components:",
                "  securitySchemes:",
                "    OIDC: {type: openIdConnect, openIdConnectUrl: 'https://x/.well-known'}",
                "paths: {}"));
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        SecurityScheme s = parser.getSecuritySchemes().get("OIDC");
        assertThat(s.getType()).isEqualTo(SecuritySchemeType.OPEN_ID_CONNECT);
    }

    @Test
    void getUnknownMethodReturnsNull() throws IOException {
        OpenAPIParser parser = new OpenAPIParser(
                Path.of("src/test/resources/test-openapi.yaml").toAbsolutePath().toString());
        assertThat(parser.getMethod("doesNotExist")).isNull();
    }

    @Test
    void specWithoutPathsParsesCleanly(@TempDir Path dir) throws IOException {
        Path p = writeSpec(dir, "openapi: '3.0.0'\ninfo: {title: t, version: '1'}\n");
        OpenAPIParser parser = new OpenAPIParser(p.toString());
        assertThat(parser.getMethods()).isEmpty();
        assertThat(parser.getServers()).isEmpty();
        assertThat(parser.getDefaultSecurity()).isEmpty();
    }
}
