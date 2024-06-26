#pragma once

#include "signal/input.h"
#include "signal/signal.h"

namespace reactive
{
    template <typename T, typename U>
    auto sendValue(signal::Signal<T, U> sig, signal::InputHandle<T> handle)
    {
        return std::move(sig).map([handle=std::move(handle)](T value) mutable
                {
                    handle.set(std::move(value));
                });
    }
} // namespace reactive

