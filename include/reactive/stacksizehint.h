#pragma once

#include "sizehint.h"
#include "reactivevisibility.h"

namespace reactive
{
    struct REACTIVE_EXPORT StackSizeHint
    {
        SizeHintResult getWidth() const;
        SizeHintResult getHeight(float width) const;
        SizeHintResult getFinalWidth(float width, float height) const;

        std::vector<SizeHint> hints_;
    };

    REACTIVE_EXPORT StackSizeHint stackSizeHints(std::vector<SizeHint> hints);
} // namespace reactive

