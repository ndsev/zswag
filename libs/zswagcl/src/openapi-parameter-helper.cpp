#include "private/openapi-parameter-helper.hpp"

#include "base64.hpp"

#include <functional>
#include <optional>

using namespace std::string_literals;

namespace zswagcl
{
namespace impl
{

std::string formatBuffer(Format f, const std::uint8_t* ptr, std::size_t size)
{
    static_assert(std::is_integral_v<unsigned char>);
    switch (f) {
    case Format::Hex:
        return stx::to_hex(ptr, ptr + size);

    case Format::Base64:
        return base64_encode(ptr, size);

    case Format::Base64url:
        return base64url_encode(ptr, size);

    case Format::Binary:
    case Format::String:
        return std::string(ptr, ptr + size);
    }

    return {};
}

}

using Style = OpenAPIConfig::Parameter::Style;

namespace
{

template <class... _T>
struct Overloaded : _T...
{
    using _T::operator()...;
};

template <class... _T>
Overloaded(_T...) -> Overloaded<_T...>;

template <class _Result, class _Variant>
_Result visitValue(const _Variant& v,
                   _Result defaultValue,
                   std::function<std::optional<_Result>(const std::string&)> single,
                   std::function<std::optional<_Result>(const std::vector<std::string>&)> vector,
                   std::function<std::optional<_Result>(const std::map<std::string, std::string>&)> map)
{
    auto result = defaultValue;

    std::visit(Overloaded {
        [&](const std::string& v) {
            if (auto res = single(v))
                result = *res;
        },
        [&](const std::vector<std::string>& v) {
            if (auto res = vector(v))
                result = *res;
        },
        [&](const std::map<std::string, std::string>& v) {
            if (auto res = map(v))
                result = *res;
        }
    }, v);

    return result;
}

std::string joinMap(const std::map<std::string, std::string>& map,
                    const std::string& kvSeparator,
                    const std::string& pairSeparator)
{
    std::vector<std::string> tmp;
    tmp.resize(map.size());

    std::transform(map.begin(), map.end(), tmp.begin(), [&](const auto& kv) {
        return kv.first + kvSeparator + kv.second;
    });

    return stx::join(tmp.begin(), tmp.end(), pairSeparator);
}

}

std::string ParameterValue::bodyStr() const
{
    return visitValue<std::string>(value, {},
        [&](const std::string& v) -> std::optional<std::string> {
            return v;
        },
        [&](const std::vector<std::string>&) -> std::optional<std::string> {
            throw std::runtime_error("Expected parameter-value of type string, got vector");
        },
        [&](const std::map<std::string, std::string>&) -> std::optional<std::string> {
            throw std::runtime_error("Expected parameter-value of type string, got dictionary");
        });
}

std::string ParameterValue::pathStr(const OpenAPIConfig::Parameter& param) const
{
    return visitValue<std::string>(value, param.defaultValue,
        [&](const std::string& v) -> std::optional<std::string> {
            switch (param.style) {
            case Style::Simple:
                return v;
            case Style::Label:
                return "."s + v;
            case Style::Matrix:
                return ";"s + param.ident + "="s + v;
            default:
                return {};
            }
        },
        [&](const std::vector<std::string>& v) -> std::optional<std::string> {
            switch (param.style) {
            case Style::Simple:
                return stx::join(v.begin(), v.end(), ",");
            case Style::Label:
                if (param.explode)
                    return "."s + stx::join(v.begin(), v.end(), ".");
                return "."s + stx::join(v.begin(), v.end(), ",");
            case Style::Matrix:
                if (param.explode)
                    return ";"s + param.ident + "=" + stx::join(v.begin(), v.end(), ";"s + param.ident + "=");
                return ";"s + param.ident + "=" + stx::join(v.begin(), v.end(), ",");
            default:
                return {};
            }
        },
        [&](const std::map<std::string, std::string>& v) -> std::optional<std::string> {
            switch (param.style) {
            case Style::Simple:
                if (param.explode)
                    return joinMap(v, "=", ",");
                else
                    return joinMap(v, ",", ",");
            case Style::Label:
                if (param.explode)
                    return "."s + joinMap(v, "=", ".");
                else
                    return "."s + joinMap(v, ",", ",");
            case Style::Matrix:
                if (param.explode)
                    return ";"s + joinMap(v, "=", ";");
                else
                    return ";"s + param.ident + "=" + joinMap(v, ",", ",");
            default:
                return {};
            }
        }
    );

}

std::vector<std::pair<std::string, std::string>> ParameterValue::queryOrHeaderPairs(const OpenAPIConfig::Parameter& param) const
{
    using Pair = std::pair<std::string, std::string>;
    using List = std::vector<Pair>;

    return visitValue<List>(value, {},
        [&](const std::string& v) -> std::optional<List> {
            switch (param.style) {
            case Style::Form:
                return {{{param.ident, v}}};
            default:
                return {};
            }
        },
        [&](const std::vector<std::string>& v) -> std::optional<List> {
            switch (param.style) {
            case Style::Form:
                /* Result example: ?id=1&id=2&id=3*/
                if (param.explode) {
                    List tmp;
                    tmp.resize(v.size());
                    std::transform(v.begin(), v.end(), tmp.begin(), [&](const auto& v) {
                        return std::make_pair(param.ident, v);
                    });

                    return tmp;
                }
                return {{std::make_pair(param.ident, stx::join(v.begin(), v.end(), ","))}};
            default:
                return {};
            }
        },
        [&](const std::map<std::string, std::string>& v) -> std::optional<List> {
            switch (param.style) {
            case Style::Form:
                if (param.explode)
                    return List(v.begin(), v.end());
                return {{std::make_pair(param.ident, joinMap(v, ",", ","))}};
            default:
                return {};
            }
        }
    );
}

}
