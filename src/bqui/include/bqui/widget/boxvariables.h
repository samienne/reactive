#pragma once

#include "bqui/bquivisibility.h"

#include <arrange/expression.h>
#include <arrange/variable.h>

namespace bqui::widget
{
    /**
     * @brief The four edge variables bounding one widget's box in the shared
     * constraint system.
     *
     * A widget holds its BoxVariables for its whole lifetime, so the arrange
     * ids stay stable across solves and the solver's incremental diff can match
     * the widget's constraints from pass to pass. A default-constructed set
     * mints four fresh ids; copying preserves them, so a copy names the same
     * box.
     */
    struct BoxVariables
    {
        arrange::Variable left;
        arrange::Variable top;
        arrange::Variable right;
        arrange::Variable bottom;

        BQUI_EXPORT arrange::Expression width() const;
        BQUI_EXPORT arrange::Expression height() const;
    };
} // namespace bqui::widget
