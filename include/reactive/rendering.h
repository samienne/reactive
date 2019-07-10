#pragma once

#include "reactivevisibility.h"

#include <avg/drawing.h>
#include <avg/painter.h>

#include <ase/vector.h>
#include <ase/commandbuffer.h>
#include <ase/framebuffer.h>

#include <btl/variant.h>

#include <pmr/memory_resource.h>

namespace reactive
{
    using RenderElement = avg::Drawing::Element;

    REACTIVE_EXPORT void render(
            pmr::memory_resource* memory,
            ase::CommandBuffer& queue,
            ase::RenderContext& context,
            ase::Framebuffer& framebuffer,
            ase::Vector2i size,
            avg::Painter const& painter,
            avg::Drawing const& drawing);
}

