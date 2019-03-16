#pragma once

#include "sizehint.h"

namespace reactive
{
    struct StackSizeHint
    {
        SizeHintResult operator()() const
        {
            auto hints = btl::fmap(hints_, [](auto const& hint)
                {
                    return hint();
                });

            return getLargestHint(hints);
        }

        SizeHintResult operator()(float x) const
        {
            auto hints = btl::fmap(hints_, [x](auto const& hint)
                {
                    return hint(x);
                });

            return getLargestHint(hints);
        }

        SizeHintResult operator()(float x, float y) const
        {
            auto hints = btl::fmap(hints_, [x, y](auto const& hint)
                {
                    return hint(x, y);
                });

            return getLargestHint(hints);
        }

        std::vector<SizeHint> const hints_;
    };

    inline auto stackSizeHints(std::vector<SizeHint> const& hints)
        -> StackSizeHint
    {
        return { hints };
    }
} // namespace reactive

