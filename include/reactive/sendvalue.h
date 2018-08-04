#pragma once

#include "signal/inputhandle.h"
#include "signal/map.h"
#include "signal.h"

namespace reactive
{
    template <typename T, typename U>
    auto sendValue(Signal<T, U> sig, signal::InputHandle<T> handle)
    {
        return signal::map([handle=std::move(handle)](T value) mutable
                {
                    handle.set(std::move(value));
                }, std::move(sig));
    }
} // namespace reactive

