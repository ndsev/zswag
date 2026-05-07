package io.github.ndsev.zswag.shared;

import io.github.ndsev.zswag.api.*;
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
public class OpenAPIClient implements IOpenAPIClient {
    private static final Logger logger = LoggerFactory.getLogger(OpenAPIClient.class);

    /** zswag MIME type for both request bodies and response Accept header. */
    public static final String ZSERIO_OBJECT_CONTENT_TYPE = "application/x-zserio-object";

    private final String specLocation;
    private final IHttpClient httpClient;
    private final HttpConfig adhoc;
    private final IKeychain keychain;
    private final OpenAPIParser parser;
    private final String baseUrl;

    public OpenAPIClient(@NotNull String specLocation, @NotNull IHttpClient httpClient,
                         @NotNull IKeychain keychain) throws IOException {
        this(specLocation, httpClient, HttpConfig.empty(), keychain);
    }

    public OpenAPIClient(@NotNull String specLocation, @NotNull IHttpClient httpClient,
                         @NotNull HttpConfig adhoc, @NotNull IKeychain keychain) throws IOException {
        this.specLocation = specLocation;
        this.httpClient = httpClient;
        this.adhoc = adhoc;
        this.keychain = keychain;
        this.parser = parseSpec(specLocation, httpClient, adhoc, keychain);
        this.baseUrl = resolveBaseUrl();
    }

    /**
     * Parses the OpenAPI spec, optionally pre-acquiring an OAuth2 access token
     * and injecting it as an {@code Authorization: Bearer} header on the spec
     * fetch request. This is required when the spec endpoint itself sits
     * behind OAuth2 (the user opts in via
     * {@link HttpConfig.OAuth2#useForSpecFetch}, default {@code true}).
     *
     * <p>When {@code useForSpecFetch=true} but the merged config lacks
     * {@code tokenUrl} (we have nothing else to fall back on at this point
     * — the spec hasn't been parsed yet so its {@code flows.clientCredentials.tokenUrl}
     * is unknown), throws {@link IOException} with a descriptive message
     * rather than silently fetching unauthenticated and 401-ing.
     */
    @NotNull
    private static OpenAPIParser parseSpec(@NotNull String specLocation, @NotNull IHttpClient httpClient,
                                           @NotNull HttpConfig adhoc, @NotNull IKeychain keychain) throws IOException {
        HttpConfig effective = httpClient.getPersistentSettings().forUrl(specLocation).mergedWith(adhoc);
        HttpConfig.OAuth2 oauth = effective.getOAuth2().orElse(null);
        if (oauth == null || !oauth.useForSpecFetch) {
            return new OpenAPIParser(specLocation);
        }
        if (oauth.tokenUrlOverride.isEmpty()) {
            throw new IOException("OAuth2 useForSpecFetch=true requires oauth2.tokenUrl in http-settings "
                    + "(the spec has not been fetched yet so its flows.clientCredentials.tokenUrl is "
                    + "unknown). Either set oauth2.tokenUrl, or set useForSpecFetch=false if the spec "
                    + "endpoint is publicly readable.");
        }
        try {
            OAuth2Handler handler = new OAuth2Handler(httpClient, keychain);
            String token = handler.getAccessToken(
                    oauth, oauth.tokenUrlOverride, oauth.tokenUrlOverride, oauth.scopesOverride);
            logger.debug("[OAuth2] Pre-fetch token acquired for spec endpoint {}", specLocation);
            return new OpenAPIParser(specLocation,
                    conn -> conn.setRequestProperty("Authorization", "Bearer " + token));
        } catch (HttpException e) {
            throw new IOException("OAuth2 token mint for spec fetch failed: " + e.getMessage(), e);
        }
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

        // Apply security: route api-key to the right location and mint OAuth2 tokens.
        // The merged config is needed to know which auth credentials are configured.
        HttpConfig effective = mergedConfigFor(fullUrl.toString());
        applySecurity(info, effective, opHeaders, queryPairs);

        // Re-append the (possibly-extended) query string when applySecurity added api-key/query entries.
        // Reset URL building since query may have grown.
        StringBuilder finalUrl = new StringBuilder(baseUrl);
        if (!baseUrl.isEmpty() && !baseUrl.endsWith("/") && !path.startsWith("/")) {
            finalUrl.append("/");
        }
        finalUrl.append(path);
        if (!queryPairs.isEmpty()) {
            finalUrl.append("?").append(ParameterEncoder.buildQueryString(queryPairs));
        }

        // Build the HTTP request.
        io.github.ndsev.zswag.api.HttpRequest.Builder rb = io.github.ndsev.zswag.api.HttpRequest.builder()
                .method(info.getHttpMethod())
                .url(finalUrl.toString())
                .headers(opHeaders);
        if (body != null) rb.body(body);

        io.github.ndsev.zswag.api.HttpResponse response = httpClient.execute(rb.build(), adhoc);

        // Strict 200 — matches C++ openapi-client.cpp:200.
        if (response.getStatusCode() != 200) {
            String contextDesc = "[" + info.getHttpMethod() + " " + fullUrl + "]";
            String errorMsg = contextDesc + " Got HTTP status: " + response.getStatusCode();
            throw new HttpException(errorMsg, response.getStatusCode(), response.getBody());
        }
        byte[] respBody = response.getBody();
        return respBody != null ? respBody : new byte[0];
    }

    /**
     * Computes the effective {@link HttpConfig} for a given URL: the persistent
     * settings exposed by the underlying {@link IHttpClient} (scope-matched
     * against the URL) merged with this client's adhoc config.
     */
    @NotNull
    private HttpConfig mergedConfigFor(@NotNull String url) {
        return httpClient.getPersistentSettings().forUrl(url).mergedWith(adhoc);
    }

    /**
     * Walks the operation's security alternatives and applies each scheme:
     * <ul>
     *   <li>HTTP basic / bearer: validated by the underlying {@link IHttpClient}
     *       from the merged config; throws here if neither is configured.</li>
     *   <li>API-key: routes the merged config's {@link HttpConfig#getApiKey()}
     *       to header / query / cookie based on the scheme's {@code in}.</li>
     *   <li>OAuth2: mints (or pulls cached) bearer token via
     *       {@link OAuth2Handler}, applying spec/settings precedence rules,
     *       then injects {@code Authorization: Bearer ...} into the request
     *       headers.</li>
     * </ul>
     *
     * <p>Picks the first alternative whose schemes are all present in the
     * merged config. Throws if no alternative can be satisfied.
     */
    private void applySecurity(@NotNull OpenAPIParser.MethodInfo info,
                               @NotNull HttpConfig effective,
                               @NotNull Map<String, String> opHeaders,
                               @NotNull List<Map.Entry<String, String>> queryPairs) throws HttpException {
        List<SecurityRequirement> alternatives = info.getSecurity()
                .orElse(parser.getDefaultSecurity().orElse(Collections.emptyList()));
        if (alternatives.isEmpty()) return;

        // Pick the first alternative whose schemes can be satisfied.
        Map<String, SecurityScheme> schemes = parser.getSecuritySchemes();
        List<String> failures = new ArrayList<>();
        for (SecurityRequirement alt : alternatives) {
            try {
                for (Map.Entry<String, List<String>> req : alt.getSchemes().entrySet()) {
                    SecurityScheme scheme = schemes.get(req.getKey());
                    if (scheme == null) {
                        throw new HttpException("Security scheme '" + req.getKey() + "' referenced by operation but not defined in components.securitySchemes");
                    }
                    applySingleScheme(scheme, req.getValue(), effective, opHeaders, queryPairs);
                }
                return; // all schemes in this alternative satisfied
            } catch (HttpException e) {
                failures.add(e.getMessage());
            }
        }
        throw new HttpException("Operation " + info.getOperationId() + " requires security but none of the "
                + alternatives.size() + " alternatives could be satisfied: " + failures);
    }

    private void applySingleScheme(@NotNull SecurityScheme scheme, @NotNull List<String> requiredScopes,
                                   @NotNull HttpConfig effective, @NotNull Map<String, String> opHeaders,
                                   @NotNull List<Map.Entry<String, String>> queryPairs) throws HttpException {
        switch (scheme.getType()) {
            case HTTP: {
                String s = scheme.getScheme() == null ? "" : scheme.getScheme().toLowerCase();
                if ("basic".equals(s)) {
                    if (!effective.getAuth().isPresent()) {
                        throw new HttpException("HTTP Basic auth required but no basic-auth configured");
                    }
                } else if ("bearer".equals(s)) {
                    boolean hasBearer = effective.getHeader("Authorization")
                            .map(v -> v.startsWith("Bearer "))
                            .orElse(false);
                    if (!hasBearer) {
                        throw new HttpException("HTTP Bearer auth required but no Authorization: Bearer header configured");
                    }
                }
                break;
            }
            case API_KEY: {
                String keyValue = effective.getApiKey().orElse(null);
                if (keyValue == null) {
                    // The user might have set the key directly via the matching channel.
                    // Probe for it before declaring failure.
                    keyValue = lookupConfiguredApiKey(scheme, effective);
                }
                if (keyValue == null) {
                    throw new HttpException("API-key auth required by scheme '" + scheme.getName()
                            + "' but no api-key configured (set via http-settings api-key, or directly via "
                            + scheme.getApiKeyLocation() + " '" + scheme.getApiKeyName() + "')");
                }
                if (effective.getApiKey().isPresent()) {
                    // Route the configured api-key to the appropriate location.
                    switch (scheme.getApiKeyLocation()) {
                        case HEADER:
                            opHeaders.put(scheme.getApiKeyName(), keyValue);
                            break;
                        case QUERY:
                            queryPairs.add(new java.util.AbstractMap.SimpleImmutableEntry<>(scheme.getApiKeyName(), keyValue));
                            break;
                        case COOKIE: {
                            String cookieValue = scheme.getApiKeyName() + "=" + keyValue;
                            opHeaders.merge("Cookie", cookieValue,
                                    (existing, incoming) -> existing + "; " + incoming);
                            break;
                        }
                        default:
                            break;
                    }
                }
                break;
            }
            case OAUTH2: {
                // Resolve OAuth2 config from settings (effective.oauth2) — the spec scopes/tokenUrl
                // are fallbacks when settings don't override.
                HttpConfig.OAuth2 oauth = effective.getOAuth2().orElse(null);
                if (oauth == null) {
                    throw new HttpException("OAuth2 required by scheme '" + scheme.getName()
                            + "' but no oauth2 config in HTTP settings");
                }
                String tokenUrl = !oauth.tokenUrlOverride.isEmpty()
                        ? oauth.tokenUrlOverride
                        : scheme.getTokenUrl().orElse("");
                String refreshUrl = !oauth.refreshUrlOverride.isEmpty()
                        ? oauth.refreshUrlOverride
                        : scheme.getRefreshUrl().orElse(tokenUrl);
                if (tokenUrl.isEmpty()) {
                    throw new HttpException("OAuth2 client-credentials: tokenUrl is missing in spec and http-settings");
                }
                List<String> scopes = !oauth.scopesOverride.isEmpty() ? oauth.scopesOverride : requiredScopes;

                OAuth2Handler handler = new OAuth2Handler(httpClient, keychain);
                String token = handler.getAccessToken(oauth, tokenUrl, refreshUrl, scopes);
                opHeaders.put("Authorization", "Bearer " + token);
                break;
            }
            case OPEN_ID_CONNECT:
                throw new HttpException("OpenID Connect security scheme '" + scheme.getName()
                        + "' is not supported by zswag clients");
        }
    }

    /**
     * Probes the merged config for an API-key value already supplied directly
     * via header/query/cookie (matching the scheme's location). Returns the
     * value found, or null if none.
     */
    @Nullable
    private String lookupConfiguredApiKey(@NotNull SecurityScheme scheme, @NotNull HttpConfig effective) {
        String name = scheme.getApiKeyName();
        if (name == null || scheme.getApiKeyLocation() == null) return null;
        switch (scheme.getApiKeyLocation()) {
            case HEADER:
                return effective.getHeader(name).orElse(null);
            case QUERY:
                List<String> queryVals = effective.getQuery().get(name);
                return (queryVals != null && !queryVals.isEmpty()) ? queryVals.get(0) : null;
            case COOKIE:
                return effective.getCookies().get(name);
            default:
                return null;
        }
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
