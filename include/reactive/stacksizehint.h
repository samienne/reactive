#pragma once

#include "sizehint.h"
#include "reactivevisibility.h"

namespace reactive
{
    struct REACTIVE_EXPORT StackSizeHint
    {
        SizeHintResult operator()() const;
        SizeHintResult operator()(float x) const;
        SizeHintResult operator()(float x, float y) const;

        std::vector<SizeHint> const hints_;
    };

    REACTIVE_EXPORT StackSizeHint stackSizeHints(std::vector<SizeHint> const& hints);
} // namespace reactive

