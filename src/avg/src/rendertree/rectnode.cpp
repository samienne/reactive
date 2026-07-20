#include "rendertree/rectnode.h"

#include "pathbuilder.h"
#include "shape.h"

#include <algorithm>
#include <optional>

namespace avg
{

RectNode::RectNode(
        Animated<Obb> obb,
        std::optional<AnimationOptions> animationOptions,
        Animated<float> radius,
        Animated<std::optional<Brush>> brush,
        Animated<std::optional<Pen>> pen
        ) :
    ShapeNode(std::move(obb), std::move(animationOptions), RectNode::drawRect,
            std::move(radius), std::move(brush), std::move(pen))
{
}

Drawing RectNode::drawRect(
        DrawContext const& context,
        Vector2f size,
        float radius,
        std::optional<Brush> const& brush,
        std::optional<Pen> const& pen
        )
{
    radius = std::clamp(radius, 0.0f, std::min(size[0], size[1]) / 2.0f);

    auto path = context.pathBuilder()
        .start(Vector2f(radius, 0.0f))
        .lineTo(size[0] - radius, 0.0f)
        .conicTo(Vector2f(size[0], 0.0f), Vector2f(size[0], radius))
        .lineTo(size[0], size[1] - radius)
        .conicTo(Vector2f(size[0], size[1]), Vector2f(size[0] - radius, size[1]))
        .lineTo(radius, size[1])
        .conicTo(Vector2f(0.0f, size[1]), Vector2f(0.0f, size[1] - radius))
        .lineTo(0.0f, radius)
        .conicTo(Vector2f(0.0f, 0.0f), Vector2f(radius, 0.0f))
        .build();

    return Shape(path).fillAndStroke(brush, pen);
}

} // namespace avg
