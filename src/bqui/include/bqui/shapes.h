#pragma once

#include "bquivisibility.h"

#include <avg/shape.h>

#include <ase/vector.h>

#include <pmr/memory_resource.h>

namespace bqui
{
    BQUI_EXPORT avg::Path makeRect(pmr::memory_resource* memory,
            float width, float height);
    BQUI_EXPORT avg::Path makeRoundedRect(pmr::memory_resource* memory,
            float width, float height, float radius);
    BQUI_EXPORT avg::Path makePathFromRect(pmr::memory_resource* memory,
            avg::Rect const& r, float radius = 0.0f);

    BQUI_EXPORT avg::Path makeCircle(pmr::memory_resource* memory,
            ase::Vector2f center, float radius);
}

