#pragma once

#include <fit/compose.h>

namespace btl
{
    template <typename... TFuncs>
    auto compose(TFuncs&&... funcs)
        -> decltype(fit::compose(std::forward<TFuncs>(funcs)...))
    {
        return fit::compose(std::forward<TFuncs>(funcs)...);
    }
} // btl

