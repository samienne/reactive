#pragma once

#include "signaltraits.h"

#include <utility>

namespace reactive::signal
{
    template <typename TSignal>
    auto evaluate(TSignal&& signal) -> decltype(signal.evaluate())
    {
        return std::forward<TSignal>(signal).evaluate();
    }
} // reactive::signal

