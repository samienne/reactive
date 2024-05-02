#pragma once

#include "signal2/input.h"
#include "signal2/signal.h"

namespace reactive
{
    template <typename T, typename U>
    auto sendValue(signal2::Signal<T, U> sig, signal2::InputHandle<T> handle)
    {
        return std::move(sig).map([handle=std::move(handle)](T value) mutable
                {
                    handle.set(std::move(value));
                });
    }
} // namespace reactive

