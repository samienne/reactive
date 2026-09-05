#pragma once

#include "sizehint.h"
#include "bquivisibility.h"

namespace bqui
{
    struct BQUI_EXPORT StackSizeHint
    {
        AxisHint getWidth() const;
        AxisHint getHeightForWidth(float width) const;
        AxisHint getWidthForHeight(float height) const;

        std::vector<SizeHint> hints_;
    };

    BQUI_EXPORT StackSizeHint stackSizeHints(std::vector<SizeHint> hints);
}

