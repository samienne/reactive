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

            // Source of callback ids. Lives on the control (not on a view) so
            // that every SharedStream viewing this control — including sibling
            // shares — draws from the same sequence and gets a unique id.
            size_t nextIndex = 1;
        };

    } // stream
} // reactive

