#pragma once

#include "reactive/signaltraits.h"

#include <btl/hidden.h>

#include <utility>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        auto evaluate(TSignal&& signal) -> decltype(signal.evaluate())
        {
            return std::forward<TSignal>(signal).evaluate();
        }
    } // signal
} // reactive

BTL_VISIBILITY_POP

