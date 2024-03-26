#pragma once

#include "reactivevisibility.h"

#include <avg/shape.h>

#include <ase/vector.h>

#include <pmr/memory_resource.h>

namespace reactive
{
    REACTIVE_EXPORT avg::Path makeRect(pmr::memory_resource* memory,
            float width, float height);
    REACTIVE_EXPORT avg::Path makeRoundedRect(pmr::memory_resource* memory,
            float width, float height, float radius);
    REACTIVE_EXPORT avg::Path makePathFromRect(pmr::memory_resource* memory,
            avg::Rect const& r, float radius = 0.0f);

    REACTIVE_EXPORT avg::Path makeCircle(pmr::memory_resource* memory,
            ase::Vector2f center, float radius);
} // namespace reactive

