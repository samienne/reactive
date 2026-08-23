#pragma once

#include "bqui/widget/guide.h"

#include <avg/rendertree/uniqueid.h>

namespace bqui::widget
{
    // The private door onto a guide's identity. XGuide and YGuide expose no id
    // getter, so the alignment modifiers reach the token through this friend to
    // record it on the builder; nothing in the public surface can.
    struct GuideAccess
    {
        static avg::UniqueId id(XGuide const& guide)
        {
            return guide.id_;
        }

        static avg::UniqueId id(YGuide const& guide)
        {
            return guide.id_;
        }
    };
} // namespace bqui::widget
