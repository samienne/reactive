#pragma once

#include "sizehint.h"
#include "bquivisibility.h"

namespace bqui
{
    struct BQUI_EXPORT StackSizeHint
    {
        SizeHintResult getWidth() const;
        SizeHintResult getHeightForWidth(float width) const;
        SizeHintResult getWidthForHeight(float height) const;

        std::vector<SizeHint> hints_;
    };

    BQUI_EXPORT StackSizeHint stackSizeHints(std::vector<SizeHint> hints);
}

