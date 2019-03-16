#pragma once

#include "sizehint.h"

#include <btl/visibility.h>

namespace reactive
{
    struct BTL_VISIBLE StackSizeHint
    {
        SizeHintResult operator()() const;
        SizeHintResult operator()(float x) const;
        SizeHintResult operator()(float x, float y) const;

        std::vector<SizeHint> const hints_;
    };

    BTL_VISIBLE StackSizeHint stackSizeHints(std::vector<SizeHint> const& hints);
} // namespace reactive

