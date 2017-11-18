#pragma once

#include "reactive/signal.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive::signal
{
    template <typename T, typename U>
    auto eraseType(Signal<T, U> sig) -> Signal<T, void>
    {
        return std::move(sig);
    }
}

BTL_VISIBILITY_POP

