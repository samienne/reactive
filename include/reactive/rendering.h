#pragma once

#include <avg/softmesh.h>
#include <avg/drawing.h>
#include <avg/shape.h>
#include <avg/painter.h>

#include <ase/pipeline.h>
#include <ase/rendertarget.h>
#include <ase/rendercommand.h>

#include <btl/fnv1a.h>
#include <btl/uhash.h>
#include <btl/hash.h>
#include <btl/variant.h>
#include <btl/visibility.h>

#include <vector>
#include <unordered_map>

namespace reactive
{
    using RenderElement = btl::variant<avg::Shape, avg::TextEntry>;
    using RenderCache = std::unordered_map<RenderElement,
          std::vector<avg::SoftMesh>, btl::uhash<btl::fnv1a>>;

    BTL_VISIBLE RenderCache render(ase::RenderContext& context,
            RenderCache const& cache, ase::RenderTarget& target,
            avg::Painter const& painter, avg::Drawing const& drawing);

    BTL_VISIBLE void render(ase::RenderContext& context,
            ase::RenderTarget& target, avg::Painter const& painter,
            avg::SoftMesh const& mesh);

    BTL_VISIBLE avg::Path makeRect(float width, float height);

    BTL_VISIBLE avg::Shape makeShape(avg::Path const& path,
            btl::option<avg::Brush> const& brush,
            btl::option<avg::Pen> const& pen);
}

