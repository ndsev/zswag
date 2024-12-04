#pragma once

#include "zswag/compat.hpp"

namespace httpcl {

namespace compat = zswag::compat;

using string_view = zswag::compat::string_view;

template<typename T>
using optional = zswag::compat::optional<T>;

} // namespace httpcl 