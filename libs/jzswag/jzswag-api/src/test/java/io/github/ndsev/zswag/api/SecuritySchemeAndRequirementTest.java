package io.github.ndsev.zswag.api;

import org.junit.jupiter.api.Test;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

class SecuritySchemeAndRequirementTest {

    @Test
    void httpSchemeBuilderCarriesScheme() {
        SecurityScheme s = SecurityScheme.builder("BasicHttp", SecuritySchemeType.HTTP)
                .scheme("basic")
                .build();
        assertThat(s.getName()).isEqualTo("BasicHttp");
        assertThat(s.getType()).isEqualTo(SecuritySchemeType.HTTP);
        assertThat(s.getScheme()).isEqualTo("basic");
        assertThat(s.getApiKeyLocation()).isNull();
        assertThat(s.getApiKeyName()).isNull();
        assertThat(s.getTokenUrl()).isEmpty();
        assertThat(s.getRefreshUrl()).isEmpty();
        assertThat(s.getOAuth2Scopes()).isEmpty();
    }

    @Test
    void apiKeySchemeStoresLocationAndName() {
        SecurityScheme s = SecurityScheme.builder("AK", SecuritySchemeType.API_KEY)
                .apiKeyLocation(ParameterLocation.HEADER)
                .apiKeyName("X-Api-Key")
                .build();
        assertThat(s.getApiKeyLocation()).isEqualTo(ParameterLocation.HEADER);
        assertThat(s.getApiKeyName()).isEqualTo("X-Api-Key");
    }

    @Test
    void oauth2BuilderAcceptsTokenUrlAndScopes() {
        Map<String, String> scopes = new LinkedHashMap<>();
        scopes.put("read", "Read access");
        scopes.put("write", "Write access");
        SecurityScheme s = SecurityScheme.builder("OA2", SecuritySchemeType.OAUTH2)
                .tokenUrl("https://auth/token")
                .refreshUrl("https://auth/refresh")
                .oauth2Scopes(scopes)
                .build();
        assertThat(s.getTokenUrl()).contains("https://auth/token");
        assertThat(s.getRefreshUrl()).contains("https://auth/refresh");
        assertThat(s.getOAuth2Scopes()).containsEntry("read", "Read access").containsEntry("write", "Write access");
    }

    @Test
    void oauth2EmptyTokenUrlIsTreatedAsAbsent() {
        SecurityScheme s = SecurityScheme.builder("OA2", SecuritySchemeType.OAUTH2)
                .tokenUrl("")
                .refreshUrl(null)
                .build();
        assertThat(s.getTokenUrl()).isEmpty();
        assertThat(s.getRefreshUrl()).isEmpty();
    }

    @Test
    void addOAuth2ScopeAccumulates() {
        SecurityScheme s = SecurityScheme.builder("OA2", SecuritySchemeType.OAUTH2)
                .tokenUrl("u")
                .addOAuth2Scope("a", "alpha")
                .addOAuth2Scope("b", "beta")
                .build();
        assertThat(s.getOAuth2Scopes()).containsOnlyKeys("a", "b");
    }

    @Test
    void oauth2ScopesMapIsImmutable() {
        SecurityScheme s = SecurityScheme.builder("OA2", SecuritySchemeType.OAUTH2)
                .tokenUrl("u").addOAuth2Scope("a", "alpha").build();
        assertThatThrownBy(() -> s.getOAuth2Scopes().put("x", "y"))
                .isInstanceOf(UnsupportedOperationException.class);
    }

    @Test
    void securityRequirementCopiesAndIsImmutable() {
        Map<String, List<String>> raw = new LinkedHashMap<>();
        raw.put("oauth2", Arrays.asList("scope1", "scope2"));
        raw.put("apikey", Collections.emptyList());
        SecurityRequirement req = new SecurityRequirement(raw);
        // Mutating the source map after construction must not affect the requirement
        raw.put("evil", Collections.singletonList("x"));
        assertThat(req.getSchemes()).containsOnlyKeys("oauth2", "apikey");
        // The returned map and lists are immutable
        assertThatThrownBy(() -> req.getSchemes().put("x", Collections.emptyList()))
                .isInstanceOf(UnsupportedOperationException.class);
        assertThatThrownBy(() -> req.getSchemes().get("oauth2").add("more"))
                .isInstanceOf(UnsupportedOperationException.class);
    }

    @Test
    void enumValuesAccessible() {
        // Cheap exercising of enum value lists
        assertThat(SecuritySchemeType.values()).contains(
                SecuritySchemeType.HTTP, SecuritySchemeType.API_KEY,
                SecuritySchemeType.OAUTH2, SecuritySchemeType.OPEN_ID_CONNECT);
        assertThat(SecuritySchemeType.valueOf("OAUTH2")).isEqualTo(SecuritySchemeType.OAUTH2);
        assertThat(ParameterStyle.values()).contains(
                ParameterStyle.SIMPLE, ParameterStyle.LABEL, ParameterStyle.MATRIX,
                ParameterStyle.FORM, ParameterStyle.SPACE_DELIMITED,
                ParameterStyle.PIPE_DELIMITED, ParameterStyle.DEEP_OBJECT);
        assertThat(ParameterFormat.valueOf("HEX")).isEqualTo(ParameterFormat.HEX);
        assertThat(ParameterLocation.valueOf("PATH")).isEqualTo(ParameterLocation.PATH);
    }
}
