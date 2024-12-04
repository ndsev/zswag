#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include "compat.hpp"

namespace Catch {
#if __cplusplus >= 201703L
    // In C++17 mode, Catch2 already provides StringMaker for std::string_view
#else
    // In C++14 mode, we only need StringMaker for our compatibility string_view
    template<>
    struct StringMaker<zswag::compat::string_view> {
        static std::string convert(const zswag::compat::string_view& value) {
            return std::string(value.data(), value.size());
        }
    };
#endif
} 