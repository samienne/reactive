#pragma once

#include <vector>
#include <functional>

namespace reactive
{
    namespace stream
    {
        template <typename T>
        struct SharedControl
        {
            std::vector<std::pair<std::function<void(T)>, size_t>> callbacks;
        };

    } // stream
} // reactive

