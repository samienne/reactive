#pragma once

#include "control.h"

#include <memory>
#include <vector>
#include <functional>

namespace bq
{
    namespace stream
    {
        template <typename T>
        struct SharedControl
        {
            std::vector<std::pair<std::function<void(T)>, size_t>> callbacks;
            std::shared_ptr<Control<T>> upstream;
        };

    } // stream
} // reactive

