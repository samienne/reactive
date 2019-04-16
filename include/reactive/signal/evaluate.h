#pragma once

#include "reactive/signaltraits.h"
#include "reactive/reactivevisibility.h"

#include <utility>

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

