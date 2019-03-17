#pragma once

#include "dt.h"
#include "foldp.h"
#include "constant.h"
#include "reactive/signaltraits.h"
#include "reactive/reactivevisibility.h"

namespace reactive::signal
{
    namespace detail
    {
        template <typename T>
        struct add
        {
            auto operator()(T const& a, T const& b) const -> decltype(a+b)
            {
                return a+b;
            }
        };
    }

    inline auto time() -> decltype(
            foldp(detail::add<signal_time_t>(), signal_time_t(0), dt()))
    {
        return foldp(detail::add<signal_time_t>(), signal_time_t(0), dt());
    }

} // reactive::signal

