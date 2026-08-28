#pragma once

#include "bqui/modifier/setsizehint.h"

#include "bqui/widget/widget.h"

#include "bqui/bquivisibility.h"
#include "bqui/simplesizehint.h"

namespace bqui::widget
{
    /**
     * @brief An empty widget that expands to fill its pure-solver container's
     * leftover space along the container's layout axis.
     *
     * Inside a pure-solver hbox or vbox it takes no size of its own on the
     * layout axis and instead couples to the container's shared flex variable,
     * so every filler in one container splits the slack evenly while the fixed
     * and default-sized siblings hold their sizes. With two fillers each takes
     * half the remaining room; capping one (maxWidth / maxHeight) hands its
     * surplus to the others. Outside a pure-solver region it is inert.
     */
    BQUI_EXPORT AnyWidget filler();

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

