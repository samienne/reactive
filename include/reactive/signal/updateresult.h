#pragma once

#include <btl/option.h>
#include <btl/hidden.h>

#include <chrono>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    using signal_time_t = std::chrono::microseconds;

    namespace signal
    {
        using UpdateResult = btl::option<signal_time_t>;

        inline UpdateResult min(UpdateResult const& l, UpdateResult const& r)
        {
            if (l.valid() && r.valid())
                return btl::just(std::min(*l, *r));
            else if (r.valid())
                return r;
            else
                return l;
        }
    } // signal
} // reactive

BTL_VISIBILITY_POP

