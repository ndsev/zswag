package com.ndsev.zswag.desktop;

import com.ndsev.zswag.api.*;
import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import zserio.runtime.io.Writer;

import java.io.IOException;
import java.util.*;
import java.util.function.Function;

/**
 * The Java port of the C++ {@code zswagcl::OpenApiClient} / Python
 * {@code zswag.OAClient}: dispatches OpenAPI calls described by a spec, with
 * full {@code x-zserio-request-part} request-decomposition logic.
 *
 * <p>Two entry points:
 * <ul>
 *   <li>{@link #callMethod(String, Object)} — the recommended typed API.
 *       Takes a zserio request object; uses POJO reflection (via
 *       {@link ZserioReflection}) to resolve {@code x-zserio-request-part}
 *       paths and encode each parameter into the request URL, headers,
 *       cookies, or query. The whole serialized request is sent as the body
 *       when the operation declares an {@code application/x-zserio-object}
 *       request body.</li>
 *   <li>{@link #callMethod(String, Map, byte[])} — low-level entry point
 *       where the caller has already decomposed the request into a parameter
 *       map and/or pre-serialized body bytes. Useful for non-zserio OpenAPI
 *       endpoints or for testing.</li>
 * </ul>
 */
public class DesktopOpenAPIClient implements IOpenAPIClient {
    private static final Logger logger = LoggerFactory.getLogger(DesktopOpenAPIClient.class);

    /** zswag MIME type for both request bodies and response Accept header. */
    public static final String ZSERIO_OBJECT_CONTENT_TYPE = "application/x-zserio-object";

    private final String specLocation;
    private final IHttpClient httpClient;
    private final HttpConfig adhoc;
    private final OpenAPIParser parser;
    private final String baseUrl;

    public DesktopOpenAPIClient(@NotNull String specLocation, @NotNull IHttpClient httpClient) throws IOException {
        this(specLocation, httpClient, HttpConfig.empty());
    }

    public DesktopOpenAPIClient(@NotNull String specLocation, @NotNull IHttpClient httpClient,
                                @NotNull HttpConfig adhoc) throws IOException {
        this.specLocation = specLocation;
        this.httpClient = httpClient;
        this.adhoc = adhoc;
        this.parser = new OpenAPIParser(specLocation);
        this.baseUrl = resolveBaseUrl();
    }

    @NotNull
    private String resolveBaseUrl() {
        List<String> servers = parser.getServers();
        String serverUrl = !servers.isEmpty() ? servers.get(0) : "";
        boolean isRelativeUrl = serverUrl.isEmpty() || serverUrl.startsWith("/");

        if (isRelativeUrl && specLocation.startsWith("http")) {
            try {
                java.net.URL url = new java.net.URL(specLocation);
                String protocol = url.getProtocol();
                String host = url.getHost();
                int port = url.getPort();
                String basePath = serverUrl.isEmpty() ? "" : serverUrl;
                String resolved = (port != -1)
                        ? protocol + "://" + host + ":" + port + basePath
                        : protocol + "://" + host + basePath;
                logger.info("Resolved relative server URL '{}' to: {}", serverUrl, resolved);
                return resolved;
            } catch (java.net.MalformedURLException e) {
                logger.warn("Failed to parse spec location URL: {}", e.getMessage());
                return serverUrl;
            }
        } else if (!serverUrl.isEmpty()) {
            return serverUrl;
        }
        logger.warn("No servers defined in OpenAPI spec and cannot infer from spec location");
        return "";
    }

    // ------------------------------------------------------------------------
    // Typed entry point — the canonical "Python/C++ feel" API.
    // ------------------------------------------------------------------------

    /**
     * Calls an OpenAPI method with a typed zserio request. The request is
     * decomposed into path/query/header/cookie parameters and (if the
     * operation declares it) a serialized {@code application/x-zserio-object}
     * body, per {@code x-zserio-request-part} on each parameter.
     *
     * @param methodIdent OpenAPI {@code operationId} (matches zserio method name)
     * @param zserioRequest typed zserio request object (must implement {@link Writer}
     *                      if the operation declares a request body)
     * @return raw response bytes (caller deserializes via zserio)
     */
    @NotNull
    public byte[] callMethod(@NotNull String methodIdent, @NotNull Object zserioRequest) throws HttpException {
        OpenAPIParser.MethodInfo info = parser.getMethod(methodIdent);
        if (info == null) {
            throw new HttpException("Method '" + methodIdent + "' is not part of the OpenAPI specification");
        }

        Function<OpenAPIParameter, Object> resolver = param -> {
            String requestPart = param.getRequestPart().orElse(null);
            if (requestPart == null) {
                // Parameters without x-zserio-request-part are not auto-filled by
                // the dispatch; they may be supplied by HttpConfig (e.g. an
                // API-key header). Return null to skip.
                return null;
            }
            return ZserioReflection.resolveOrSerialize(zserioRequest, requestPart);
        };

        byte[] body = null;
        if (info.hasZserioBody()) {
            if (!(zserioRequest instanceof Writer)) {
                throw new HttpException("Operation " + methodIdent + " declares a zserio request body, but "
                        + zserioRequest.getClass().getName() + " does not implement zserio.runtime.io.Writer");
            }
            body = ZserioReflection.serialize((Writer) zserioRequest);
        }

        return dispatch(info, resolver, body);
    }

    // ------------------------------------------------------------------------
    // Map-based entry point — low-level / testing.
    // ------------------------------------------------------------------------

    @Override
    @Nullable
    public byte[] callMethod(@NotNull String methodPath, @NotNull Map<String, Object> parameters,
                             @Nullable byte[] requestBody) throws HttpException {
        OpenAPIParser.MethodInfo info = parser.getMethod(methodPath);
        if (info == null) {
            throw new HttpException("Method '" + methodPath + "' is not part of the OpenAPI specification");
        }
        Function<OpenAPIParameter, Object> resolver = param -> parameters.get(param.getName());
        return dispatch(info, resolver, requestBody);
    }

    // ------------------------------------------------------------------------
    // Shared dispatch core.
    // ------------------------------------------------------------------------

    @NotNull
    private byte[] dispatch(@NotNull OpenAPIParser.MethodInfo info,
                            @NotNull Function<OpenAPIParameter, Object> resolver,
                            @Nullable byte[] body) throws HttpException {
        logger.debug("Calling {} {} ({})", info.getHttpMethod(), info.getPathTemplate(), info.getOperationId());

        String path = info.getPathTemplate();
        List<Map.Entry<String, String>> queryPairs = new ArrayList<>();
        Map<String, String> opHeaders = new LinkedHashMap<>();
        Map<String, String> opCookies = new LinkedHashMap<>();

        for (OpenAPIParameter param : info.getParameters()) {
            Object value = resolver.apply(param);
            if (value == null) {
                if (param.isRequired() && param.getRequestPart().isPresent()) {
                    throw new HttpException("Required parameter '" + param.getName()
                            + "' resolved to null via x-zserio-request-part: " + param.getRequestPart().get());
                }
                continue;
            }

            switch (param.getLocation()) {
                case PATH:
                    path = path.replace("{" + param.getName() + "}",
                            ParameterEncoder.urlEncode(ParameterEncoder.encodeForPath(param, value)));
                    break;
                case QUERY:
                    queryPairs.addAll(ParameterEncoder.encodeForQuery(param, value));
                    break;
                case HEADER:
                    opHeaders.put(param.getName(), ParameterEncoder.encodeForHeader(param, value));
                    break;
                case COOKIE:
                    opCookies.put(param.getName(), ParameterEncoder.encodeForCookie(param, value));
                    break;
            }
        }

        // Reject unfilled path placeholders rather than emitting them literally.
        if (path.matches(".*\\{[^/}]+\\}.*")) {
            throw new HttpException("Unfilled path placeholder in '" + path + "' for " + info.getOperationId());
        }

        // Build full URL.
        StringBuilder fullUrl = new StringBuilder(baseUrl);
        if (!baseUrl.isEmpty() && !baseUrl.endsWith("/") && !path.startsWith("/")) {
            fullUrl.append("/");
        }
        fullUrl.append(path);
        if (!queryPairs.isEmpty()) {
            fullUrl.append("?").append(ParameterEncoder.buildQueryString(queryPairs));
        }

        // Operation-level cookies → Cookie header (merged with persistent/adhoc cookies in HttpClient).
        if (!opCookies.isEmpty()) {
            StringJoiner sj = new StringJoiner("; ");
            for (Map.Entry<String, String> e : opCookies.entrySet()) sj.add(e.getKey() + "=" + e.getValue());
            opHeaders.merge("Cookie", sj.toString(), (existing, incoming) -> existing + "; " + incoming);
        }

        // zswag protocol headers.
        opHeaders.put("Accept", ZSERIO_OBJECT_CONTENT_TYPE);
        if (body != null) {
            opHeaders.put("Content-Type", ZSERIO_OBJECT_CONTENT_TYPE);
        }

        // Build the HTTP request.
        com.ndsev.zswag.api.HttpRequest.Builder rb = com.ndsev.zswag.api.HttpRequest.builder()
                .method(info.getHttpMethod())
                .url(fullUrl.toString())
                .headers(opHeaders);
        if (body != null) rb.body(body);

        com.ndsev.zswag.api.HttpResponse response = httpClient.execute(rb.build(), adhoc);

        // Strict 200 — matches C++ openapi-client.cpp:200.
        if (response.getStatusCode() != 200) {
            String contextDesc = "[" + info.getHttpMethod() + " " + fullUrl + "]";
            String errorMsg = contextDesc + " Got HTTP status: " + response.getStatusCode();
            throw new HttpException(errorMsg, response.getStatusCode(), response.getBody());
        }
        byte[] respBody = response.getBody();
        return respBody != null ? respBody : new byte[0];
    }

    @Override
    @NotNull
    public IHttpClient getHttpClient() {
        return httpClient;
    }

    @Override
    @NotNull
    public String getOpenAPISpecLocation() {
        return specLocation;
    }

    /** Exposes the parsed spec for callers that need to introspect operations. */
    @NotNull
    public OpenAPIParser getParser() {
        return parser;
    }
}
