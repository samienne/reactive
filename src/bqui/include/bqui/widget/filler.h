#pragma once

#include "bqui/widget/widget.h"

#include "bqui/bquivisibility.h"

namespace bqui::widget
{
    /**
     * @brief An empty widget, stating no size on either axis.
     *
     * Along a pure-solver box it grows from zero and shares the slack with the
     * other growers by weight, after any sibling free within its bounds has
     * taken what it can. Across a box, and in a stack or grid slot, it fills
     * the slot. Bounding it (maxWidth / maxHeight) makes it a free widget that
     * takes slack before the growers.
     */
    BQUI_EXPORT AnyWidget filler();

    /**
     * @brief An empty widget that grows horizontally and is pulled to zero
     * height.
     *
     * The zero height is only a pull: in a row it neither caps the row's height
     * nor takes any of it, and in a column it stays at zero height while it
     * spans the column's width.
     */
    BQUI_EXPORT AnyWidget hfiller();

    /**
     * @brief An empty widget that grows vertically and is pulled to zero
     * width; the mirror of hfiller().
     */
    BQUI_EXPORT AnyWidget vfiller();

    /**
     * @brief The same as filler(): an empty widget growing on both axes.
     */
    BQUI_EXPORT AnyWidget hwfiller();
} // namespace bqui::widget

