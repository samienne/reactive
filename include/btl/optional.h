#pragma once

#include <optional>
#include <iostream>

template <typename T>
std::ostream& operator<<(std::ostream& stream, std::optional<T> const& value)
{
    if (!value)
        return stream << "nullopt";

    return stream << "optional(" << *value << ")";
}

