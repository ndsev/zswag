#pragma once

#include "openapi-config.hpp"

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <sstream>
#include <array>

#include "stx/string.h"

namespace zswagcl
{

/**
 * Any value type container
 */
using Any = std::variant<std::int64_t, std::uint64_t, double, std::string>;

namespace impl
{

using Format = OpenAPIConfig::Parameter::Format;

/* Format buffer to string */
std::string formatBuffer(Format f, const std::uint8_t* ptr, std::size_t size);

template <class _Type, class _Enable = void>
struct FormatHelper;

template <class _Type>
struct FormatHelper<_Type, std::enable_if_t<std::is_integral_v<_Type>>>
{
    static std::string format(Format f, _Type v)
    {
        switch (f) {
        case Format::Hex: {
            std::array<char, 30> buffer;
            if (std::is_unsigned_v<_Type>)
                std::snprintf(buffer.data(), buffer.size(), "%llx",
                              (unsigned long long)v);
            else
                std::snprintf(buffer.data(), buffer.size(), "%s%llx",
                              v < _Type(0) ? "-" : "",
                              (unsigned long long)std::llabs(v));
            return {buffer.data()};
        }

        case Format::String:
            return stx::to_string(v);

        default:
            return formatBuffer(f, reinterpret_cast<const std::uint8_t*>(&v), sizeof(_Type));
        }
    }
};

template <>
struct FormatHelper<std::vector<std::uint8_t>>
{
    static std::string format(Format f, const std::vector<std::uint8_t>& v)
    {
        return formatBuffer(f, v.data(), v.size());
    }
};

template <class _Type>
struct FormatHelper<_Type, std::enable_if_t<std::is_same_v<_Type, std::string> ||
                                            std::is_same_v<_Type, const char*>>>
{
    static std::string format(Format f, std::string v)
    {
        switch (f) {
        case Format::String:
        case Format::Binary:
            return v;

        default:
            return formatBuffer(f, reinterpret_cast<const std::uint8_t*>(v.data()), v.size());
        }
    }
};

template <class _Type>
struct FormatHelper<_Type, std::enable_if_t<std::is_floating_point_v<_Type>>>
{
    static std::string format(Format f, _Type v)
    {
        switch (f) {
        case Format::String:
            return stx::to_string(v);

        default:
            return formatBuffer(f, reinterpret_cast<const std::uint8_t*>(&v), sizeof(v));
        }
    }
};

template <>
struct FormatHelper<Any>
{
    static std::string format(Format f, Any v)
    {
        if (auto value = std::get_if<std::int64_t>(&v))
            return FormatHelper<std::int64_t>::format(f, *value);
        if (auto value = std::get_if<std::uint64_t>(&v))
            return FormatHelper<std::uint64_t>::format(f, *value);
        if (auto value = std::get_if<double>(&v))
            return FormatHelper<double>::format(f, *value);
        if (auto value = std::get_if<std::string>(&v))
            return FormatHelper<std::string>::format(f, *value);

        throw std::runtime_error("Unsupported type");
    }
};

}

struct ParameterValue
{
    using ValueHolder = std::variant<std::string,
                                     std::vector<std::string>,
                                     std::map<std::string, std::string>>;

    ValueHolder value;

    ParameterValue(ValueHolder&& value)
        : value(std::move(value))
    {}

    /**
     * Returns the values string-value.
     * Throws if the current value is not a string/buffer.
     */
    std::string bodyStr() const;

    /**
     * Make path string.
     *
     * Styles supported:
     *   * Simple
     *   * Label
     *   * Matrix
     *
     * @see https://swagger.io/docs/specification/serialization/
     */
    std::string pathStr(const OpenAPIConfig::Parameter&) const;

    /**
     * Make query key-value pair.
     *
     * Styles supported:
     *   * Form
     *
     * @see https://swagger.io/docs/specification/serialization/
     */
    std::vector<std::pair<std::string, std::string>> queryPairs(const OpenAPIConfig::Parameter&) const;
};

class ParameterValueHelper
{
public:
    const OpenAPIConfig::Parameter& param;

    ParameterValueHelper(const OpenAPIConfig::Parameter& param)
        : param(param)
    {}

    template <class _Type>
    ParameterValue value(_Type&& v)
    {
        return ParameterValue(format(std::forward<_Type>(v)));
    }

    template <class _Container>
    ParameterValue array(const _Container& v)
    {
        std::vector<std::string> tmp(v.size());
        std::transform(std::begin(v), std::end(v), tmp.begin(), [&](const auto& v) {
            return format(v);
        });

        return ParameterValue(std::move(tmp));
    }

    template <class _Container>
    ParameterValue object(const _Container& v)
    {
        std::map<std::string, std::string> tmp;
        std::transform(v.begin(), v.end(), std::inserter(tmp, tmp.end()), [&](const auto& kv) {
            return std::make_pair(kv.first, format(kv.second));
        });

        return ParameterValue(std::move(tmp));
    }

    ParameterValue binary(std::vector<std::uint8_t>&& v)
    {
        return ParameterValue(format(std::move(v)));
    }

    ParameterValue binary(const std::vector<std::uint8_t>& v)
    {
        return ParameterValue(format(v));
    }

private:
    template <class _Type>
    std::string format(_Type&& v)
    {
        return impl::FormatHelper<std::decay_t<_Type>>::format(param.format, std::forward<_Type>(v));
    }
};


}
