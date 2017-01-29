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
            std::vector<std::pair<std::function<void(T)>, uint32_t>> callbacks;
        };

    } // stream
} // reactive

