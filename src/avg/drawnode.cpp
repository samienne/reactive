#include "drawnode.h"

namespace avg
{

DrawNode::DrawNode(
        Animated<Obb> obb,
        ShapeFunction shapeFunction,
        Animated<std::optional<Brush>> brush,
        Animated<std::optional<Pen>> pen,
        std::optional<AnimationOptions> animationOptions
        ) :
    RenderTreeNode(
            std::nullopt,
            std::move(obb)
            ),
    shapeFunction_(std::move(shapeFunction)),
    brush_(std::move(brush)),
    pen_(std::move(pen)),
    animationOptions_(animationOptions)
{
}

std::pair<Drawing, bool> DrawNode::draw(DrawContext const& context,
    Obb const& parentObb,
    std::chrono::milliseconds time) const
{
    auto obb = getObbAt(time);

    return std::make_pair(
        shapeFunction_(time, context, obb.getSize())
            .transform(parentObb.getTransform() * obb.getTransform())
            .fillAndStroke(brush_.getValue(time), pen_.getValue(time)),
        !shapeFunction_.hasAnimationEnded(time)
        );
}

UpdateResult DrawNode::update(RenderTree const& /*oldTree*/,
    RenderTree const& /*newTree*/,
    std::shared_ptr<RenderTreeNode> const& oldNode,
    std::shared_ptr<RenderTreeNode> const& newNode,
    std::optional<AnimationOptions> const& animationOptions,
    std::chrono::milliseconds time) const
{
    if (oldNode && !newNode)
    {
        // Disappear
        return { nullptr, std::nullopt };
    }
    else if (!oldNode && newNode)
    {
        // Appear
        return { std::move(newNode), std::nullopt };
    }

    auto const& oldDraw = reinterpret_cast<DrawNode const&>(*oldNode);
    auto const& newDraw = reinterpret_cast<DrawNode const&>(*newNode);

    auto newAnimationOptions = newDraw.animationOptions_
        ? newDraw.animationOptions_
        : animationOptions
        ;

    return {
        std::make_shared<DrawNode>(
                oldNode->getObb().updated(newNode->getObb(),
                    newAnimationOptions, time),
                newDraw.shapeFunction_.updated(oldDraw.shapeFunction_,
                    newAnimationOptions, time),
                newDraw.brush_.updated(oldDraw.brush_, newAnimationOptions, time),
                newDraw.pen_.updated(oldDraw.pen_, newAnimationOptions, time),
                newDraw.animationOptions_
                ),
            std::nullopt
    };
}

std::type_index DrawNode::getType() const
{
    return typeid(DrawNode);
}

std::shared_ptr<RenderTreeNode> DrawNode::clone() const
{
    return std::make_shared<DrawNode>(*this);
}

} // namespace avg

