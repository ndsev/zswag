package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.HttpSettings;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.yaml.snakeyaml.Yaml;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.time.Duration;
import java.util.Map;

/**
 * Loads HTTP settings from YAML configuration files and environment variables.
 */
public class ConfigurationLoader {
    private static final Logger logger = LoggerFactory.getLogger(ConfigurationLoader.class);

    private static final String ENV_SETTINGS_FILE = "HTTP_SETTINGS_FILE";
    private static final String ENV_TIMEOUT = "HTTP_TIMEOUT";
    private static final String ENV_SSL_STRICT = "HTTP_SSL_STRICT";
    private static final String ENV_BEARER_TOKEN = "HTTP_BEARER_TOKEN";

    /**
     * Loads settings from the default location.
     * Checks HTTP_SETTINGS_FILE environment variable first, then standard locations.
     */
    @NotNull
    public static HttpSettings loadSettings() throws IOException {
        String settingsFile = System.getenv(ENV_SETTINGS_FILE);
        if (settingsFile != null && !settingsFile.isEmpty()) {
            logger.info("Loading HTTP settings from: {}", settingsFile);
            return loadFromFile(settingsFile);
        }

        // No file specified, create default settings with environment overrides
        return loadFromEnvironment();
    }

    /**
     * Loads settings from a specific YAML file.
     */
    @NotNull
    @SuppressWarnings("unchecked")
    public static HttpSettings loadFromFile(@NotNull String filePath) throws IOException {
        try (InputStream input = Files.newInputStream(Paths.get(filePath))) {
            Yaml yaml = new Yaml();
            Map<String, Object> config = yaml.load(input);

            HttpSettings.Builder builder = HttpSettings.builder();

            // Load headers
            Map<String, String> headers = (Map<String, String>) config.get("headers");
            if (headers != null) {
                builder.headers(headers);
            }

            // Load query parameters
            Map<String, String> queryParams = (Map<String, String>) config.get("queryParameters");
            if (queryParams != null) {
                builder.queryParameters(queryParams);
            }

            // Load cookies
            Map<String, String> cookies = (Map<String, String>) config.get("cookies");
            if (cookies != null) {
                builder.cookies(cookies);
            }

            // Load timeout
            Integer timeout = (Integer) config.get("timeout");
            if (timeout != null) {
                builder.timeout(Duration.ofSeconds(timeout));
            }

            // Load SSL strict mode
            Boolean sslStrict = (Boolean) config.get("sslStrict");
            if (sslStrict != null) {
                builder.sslStrict(sslStrict);
            }

            // Load proxy
            String proxyUrl = (String) config.get("proxyUrl");
            if (proxyUrl != null) {
                builder.proxyUrl(proxyUrl);
            }

            // Load basic auth
            Map<String, String> basicAuth = (Map<String, String>) config.get("basicAuth");
            if (basicAuth != null) {
                String username = basicAuth.get("username");
                String password = basicAuth.get("password");
                if (username != null && password != null) {
                    builder.basicAuth(username, password);
                }
            }

            // Load bearer token
            String bearerToken = (String) config.get("bearerToken");
            if (bearerToken != null) {
                builder.bearerToken(bearerToken);
            }

            // Load API keys
            Map<String, String> apiKeys = (Map<String, String>) config.get("apiKeys");
            if (apiKeys != null) {
                builder.apiKeys(apiKeys);
            }

            // Apply environment variable overrides
            return applyEnvironmentOverrides(builder).build();
        }
    }

    /**
     * Loads settings from environment variables only.
     */
    @NotNull
    public static HttpSettings loadFromEnvironment() {
        HttpSettings.Builder builder = HttpSettings.builder();
        return applyEnvironmentOverrides(builder).build();
    }

    /**
     * Applies environment variable overrides to the builder.
     */
    @NotNull
    private static HttpSettings.Builder applyEnvironmentOverrides(@NotNull HttpSettings.Builder builder) {
        // Timeout override
        String timeoutStr = System.getenv(ENV_TIMEOUT);
        if (timeoutStr != null && !timeoutStr.isEmpty()) {
            try {
                int seconds = Integer.parseInt(timeoutStr);
                builder.timeout(Duration.ofSeconds(seconds));
                logger.debug("Applied timeout override: {}s", seconds);
            } catch (NumberFormatException e) {
                logger.warn("Invalid timeout value in environment: {}", timeoutStr);
            }
        }

        // SSL strict override
        String sslStrictStr = System.getenv(ENV_SSL_STRICT);
        if (sslStrictStr != null && !sslStrictStr.isEmpty()) {
            boolean sslStrict = "1".equals(sslStrictStr) || "true".equalsIgnoreCase(sslStrictStr);
            builder.sslStrict(sslStrict);
            logger.debug("Applied SSL strict override: {}", sslStrict);
        }

        // Bearer token override
        String bearerToken = System.getenv(ENV_BEARER_TOKEN);
        if (bearerToken != null && !bearerToken.isEmpty()) {
            builder.bearerToken(bearerToken);
            logger.debug("Applied bearer token override from environment");
        }

        return builder;
    }
}
