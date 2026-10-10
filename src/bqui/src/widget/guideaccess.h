#pragma once

#include "bqui/widget/guide.h"

#include <arrange/variable.h>

namespace bqui::widget
{
    // Guides expose no variable in the public surface; the binding modifiers
    // reach it through this friend.
    struct GuideAccess
    {
        static arrange::Variable const& variable(XGuide const& guide)
        {
            return guide.variable_;
        }

        static arrange::Variable const& variable(YGuide const& guide)
        {
            return guide.variable_;
        }
    };
} // namespace bqui::widget
