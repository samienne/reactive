#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <vector>

namespace bqui::widget
{
    /**
     * @brief Lays widgets over each other, first at the bottom, each given
     * the whole container.
     *
     * @code
     * stack({ background(), label("On top") })
     * @endcode
     *
     * The container is sized to the largest child. Each child above the
     * first paints over the ones below it, so a backdrop effect in a child
     * sees the children under it.
     */
    BQUI_EXPORT AnyWidget stack(std::vector<AnyWidget> widgets);
} // namespace bqui::widget
