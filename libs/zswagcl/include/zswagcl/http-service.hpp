// Copyright (c) Navigation Data Standard e.V. - See LICENSE file.
#pragma once

#include "httpcl/uri.hpp"
#include "httpcl/http-client.hpp"

#include <zserio/IService.h>

#include <map>
#include <memory>
#include <vector>
#include <string>

namespace zsr
{
class Variant;
}

namespace zswagcl
{

class HTTPService : public zserio::IService
{
public:
    struct Config;

    /**
     * Creates a new http service with the configuration specified.
     */
    explicit HTTPService(const Config& cfg,
                         std::unique_ptr<httpcl::IHttpClient> client);

    ~HTTPService() override;

    /**
     * Calls the configured service method with the given name, resolving
     * all parameters.
     *
     * @param context  Pointer to object of type zsr::ServiceMethod::Context
     */
    void callMethod(const std::string& methodName,
                    const std::vector<uint8_t>& requestData,
                    std::vector<uint8_t>& responseData,
                    void* context) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

struct HTTPService::Config
{
    struct Parameter {
        enum Location {
            Path,
            Query,
        } location = Query;

        /**
         * Zserio structure field or function identifier.
         * The special identifier '*' represents the binary
         * encoded request object.
         */
        std::string field;

        /**
         * Default parameter value.
         * Used if value could not be read.
         */
        std::string defaultValue;

        /**
         * Parameter encoding format.
         */
        enum Format {
            /**
             * Default encoding.
             */
            String,

            /**
             * Hexadicamal (hexpair per octet) encoding.
             * Does not include a prefix.
             */
            Hex,

            /**
             * Base64 encoding.
             */
            Base64,    // Standard
            Base64url, // URL safe

            /**
             * Binary (octet) encoding
             */
            Binary,
        } format = String;

        /**
         * Parameter style.
         *
         * https://tools.ietf.org/html/rfc6570#section-3.2.7
         */
        enum Style {
            /**
             * Simple style parameter defined by RFC 6570.
             */
            Simple,

            /**
             * Form style parameter defined by RFC 6570.
             */
            Form,

            /**
             * Path style parameter defined by RFC 6570.
             */
            Matrix,
        } style = Simple; // TODO: Not used

        /**
         * If true, generate separate parameters for each array-value
         * or object field-value.
         */
        bool explode = false;
    };

    struct Path {
        /**
         * URI suffix.
         */
        std::string path;

        /**
         * HTTP method.
         */
        std::string httpMethod = "POST";

        /**
         * Parameter name to configuration.
         */
        std::map<std::string, Parameter> parameters;

        /**
         * Zserio structure field or function identifier that is transfered
         * as request body.
         * The special identifier '*' represents the binary
         * encoded request object.
         *
         * Ignored if HTTP-Method is GET.
         */
        std::string bodyField = "*"; // TODO: Not used
    };

    /**
     * URI parts.
     */
    httpcl::URIComponents uri;

    /**
     * Map from service method name to path configuration.
     */
    std::map<std::string, Path> methodPath;
};

}
