#pragma once

#include "bqui/modifier/setsizehint.h"

#include "bqui/widget/widget.h"

#include "bqui/simplesizehint.h"

namespace bqui::widget
{
    // A filler carries a positive grow weight on the axis it fills, so it draws
    // a share of its container's leftover space; the large max keeps the
    // container's cross-axis placement from clamping it back below that space.
    inline auto hwfiller() -> AnyWidget
    {
        return makeWidget()
            | modifier::setSizeHint(bq::signal::constant(simpleSizeHint(
                        Band{0, 0, 100000, 1}, Band{0, 0, 100000, 1})));
    }

    inline auto hfiller() -> widget::AnyWidget
    {
        return makeWidget()
            | modifier::setSizeHint(bq::signal::constant(simpleSizeHint(
                        Band{0, 0, 100000, 1}, Band{0, 0, 0, 0})));
    }

    inline auto vfiller() -> widget::AnyWidget
    {
        return makeWidget()
            | modifier::setSizeHint(bq::signal::constant(simpleSizeHint(
                        Band{0, 0, 0, 0}, Band{0, 0, 100000, 1})));
    }
} // namespace bqui::widget

