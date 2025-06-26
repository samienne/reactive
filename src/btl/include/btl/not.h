#pragma once

#include <type_traits>

namespace btl
{
    template <typename T>
    struct Not : std::integral_constant<bool, !T::value> {};
} // btl

