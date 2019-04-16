#pragma once

#include "reactivevisibility.h"

#include <avg/softmesh.h>
#include <avg/drawing.h>
#include <avg/shape.h>
#include <avg/painter.h>

#include <ase/vector.h>
#include <ase/commandbuffer.h>
#include <ase/pipeline.h>
#include <ase/framebuffer.h>
#include <ase/rendercommand.h>

#include <btl/fnv1a.h>
#include <btl/uhash.h>
#include <btl/hash.h>
#include <btl/variant.h>

#include <vector>
#include <unordered_map>

namespace reactive
{
    //using RenderElement = btl::variant<avg::Shape, avg::TextEntry>;
    using RenderElement = avg::Drawing::Element;
    /*using RenderCache = std::unordered_map<RenderElement,
          std::vector<avg::SoftMesh>, btl::uhash<btl::fnv1a>>;*/

    REACTIVE_EXPORT void render(
            ase::CommandBuffer& queue,
            ase::RenderContext& context,
            ase::Framebuffer& framebuffer,
            ase::Vector2i size,
            avg::Painter const& painter,
            avg::Drawing const& drawing);

    REACTIVE_EXPORT avg::Path makeRect(float width, float height);

    REACTIVE_EXPORT avg::Shape makeShape(avg::Path const& path,
            btl::option<avg::Brush> const& brush,
            btl::option<avg::Pen> const& pen);
}

