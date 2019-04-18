#pragma once

#include "reactivevisibility.h"

#include <avg/shape.h>

namespace reactive
{
    REACTIVE_EXPORT avg::Path makeRect(float width, float height);
    REACTIVE_EXPORT avg::Path makeRoundedRect(float width, float height,
            float radius);
    REACTIVE_EXPORT avg::Path makePathFromRect(avg::Rect const& r,
            float radius = 0.0f);

    REACTIVE_EXPORT avg::Shape makeShape(avg::Path const& path,
            btl::option<avg::Brush> const& brush,
            btl::option<avg::Pen> const& pen);
} // namespace reactive

