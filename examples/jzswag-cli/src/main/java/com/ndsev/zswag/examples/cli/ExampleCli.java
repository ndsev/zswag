package com.ndsev.zswag.examples.cli;

import com.ndsev.zswag.api.*;
import com.ndsev.zswag.desktop.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/**
 * Example CLI application demonstrating jzswag-desktop usage.
 *
 * Usage:
 *   java -jar jzswag-cli.jar <openapi-spec-url> <method-path> [param=value...]
 *
 * Example:
 *   java -jar jzswag-cli.jar https://api.example.com/openapi.yaml /users userId=123
 */
public class ExampleCli {
    private static final Logger logger = LoggerFactory.getLogger(ExampleCli.class);

    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Usage: jzswag-cli <openapi-spec> <method-path> [param=value...]");
            System.err.println();
            System.err.println("Examples:");
            System.err.println("  jzswag-cli https://api.example.com/openapi.yaml /users");
            System.err.println("  jzswag-cli openapi.yaml /users/{id} id=123");
            System.err.println();
            System.err.println("Environment Variables:");
            System.err.println("  HTTP_SETTINGS_FILE  - Path to HTTP settings YAML file");
            System.err.println("  HTTP_TIMEOUT        - Request timeout in seconds");
            System.err.println("  HTTP_SSL_STRICT     - Enable strict SSL verification (0/1)");
            System.exit(1);
        }

        String specLocation = args[0];
        String methodPath = args[1];

        try {
            // Persistent HTTP settings come from HTTP_SETTINGS_FILE (loaded inside DesktopHttpClient).
            HttpSettings persistent = HttpSettingsLoader.loadFromEnvironment();
            logger.info("Loaded {} scoped HTTP setting entries", persistent.getEntries().size());

            // Parse parameters from command line
            Map<String, Object> parameters = new HashMap<>();
            for (int i = 2; i < args.length; i++) {
                String[] parts = args[i].split("=", 2);
                if (parts.length == 2) {
                    parameters.put(parts[0], parts[1]);
                    logger.info("Parameter: {} = {}", parts[0], parts[1]);
                }
            }

            logger.info("Creating HTTP client...");
            IHttpClient httpClient = new DesktopHttpClient(persistent);

            // Create OpenAPI client
            logger.info("Loading OpenAPI spec from: {}", specLocation);
            IOpenAPIClient client = new DesktopOpenAPIClient(specLocation, httpClient);

            // Call the method
            logger.info("Calling method: {}", methodPath);
            byte[] response = client.callMethod(methodPath, parameters, null);

            // Display response
            if (response != null && response.length > 0) {
                System.out.println("\n=== Response ===");
                // Try to display as string if it looks like text
                String responseStr = new String(response, StandardCharsets.UTF_8);
                if (isPrintable(responseStr)) {
                    System.out.println(responseStr);
                } else {
                    System.out.println("Binary response (" + response.length + " bytes)");
                    System.out.println("Hex: " + bytesToHex(response, 64));
                }
                System.out.println("\n=== Success ===");
            } else {
                System.out.println("\n=== Empty Response ===");
            }

        } catch (HttpException e) {
            logger.error("HTTP error: {}", e.getMessage());
            if (e.getStatusCode() != null) {
                System.err.println("HTTP " + e.getStatusCode() + ": " + e.getMessage());
            } else {
                System.err.println("Error: " + e.getMessage());
            }
            if (e.getResponseBody() != null) {
                System.err.println("\nResponse body:");
                System.err.println(new String(e.getResponseBody(), StandardCharsets.UTF_8));
            }
            System.exit(1);

        } catch (Exception e) {
            logger.error("Unexpected error", e);
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }

    private static boolean isPrintable(String str) {
        return str.chars().allMatch(c -> c >= 32 && c < 127 || Character.isWhitespace(c));
    }

    private static String bytesToHex(byte[] bytes, int maxLength) {
        StringBuilder hex = new StringBuilder();
        int length = Math.min(bytes.length, maxLength);
        for (int i = 0; i < length; i++) {
            hex.append(String.format("%02x", bytes[i]));
            if ((i + 1) % 16 == 0 && i < length - 1) {
                hex.append("\n");
            } else if (i < length - 1) {
                hex.append(" ");
            }
        }
        if (bytes.length > maxLength) {
            hex.append("\n... (").append(bytes.length - maxLength).append(" more bytes)");
        }
        return hex.toString();
    }
}
