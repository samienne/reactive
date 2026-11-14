#include "rendertree/surfacenode.h"

#include "rendertree/updateresult.h"

#include "obb.h"
#include "pen.h"
#include "shape.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <utility>

namespace avg
{

namespace
{

SurfaceNode const& asSurface(std::shared_ptr<RenderTreeNode> const& node)
{
    return reinterpret_cast<SurfaceNode const&>(*node);
}

std::optional<SurfaceShadow> updatedShadow(
        std::optional<SurfaceShadow> const& oldShadow,
        std::optional<SurfaceShadow> const& newShadow,
        std::optional<AnimationOptions> const& options,
        std::chrono::milliseconds time
        )
{
    if (!oldShadow || !newShadow)
        return newShadow;

    return SurfaceShadow{
        oldShadow->offset.updated(newShadow->offset, options, time),
        oldShadow->spread.updated(newShadow->spread, options, time),
        oldShadow->brush.updated(newShadow->brush, options, time)
    };
}

bool hasShadowAnimationEnded(std::optional<SurfaceShadow> const& shadow,
        std::chrono::milliseconds time)
{
    return !shadow
        || (shadow->offset.hasAnimationEnded(time)
                && shadow->spread.hasAnimationEnded(time)
                && shadow->brush.hasAnimationEnded(time));
}

UpdateResult updatedChild(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldChild,
        std::shared_ptr<RenderTreeNode> const& newChild,
        std::optional<AnimationOptions> const& options,
        std::chrono::milliseconds time
        )
{
    if (!oldChild && !newChild)
        return { nullptr, std::nullopt };

    if (!oldChild)
        return newChild->update(oldTree, newTree, nullptr, newChild, options,
                time);

    if (!newChild)
        return oldChild->update(oldTree, newTree, oldChild, nullptr, options,
                time);

    if (oldChild->getId() != newChild->getId())
        return newChild->update(oldTree, newTree, nullptr, newChild, options,
                time);

    return newChild->update(oldTree, newTree, oldChild, newChild, options,
            time);
}

} // anonymous namespace

SurfaceNode::SurfaceNode(
        Animated<Obb> obb,
        std::optional<AnimationOptions> animationOptions,
        ShapeFunction shape,
        Animated<std::optional<Brush>> fill,
        std::optional<SurfaceShadow> shadow,
        std::shared_ptr<RenderTreeNode> childNode
        ) :
    RenderTreeNode(std::nullopt, std::move(obb)),
    animationOptions_(std::move(animationOptions)),
    shape_(std::move(shape)),
    fill_(std::move(fill)),
    shadow_(std::move(shadow)),
    childNode_(std::move(childNode))
{
}

UpdateResult SurfaceNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        std::optional<AnimationOptions> const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    if (!oldNode && !newNode)
        return { nullptr, std::nullopt };

    if (!oldNode)
    {
        // Appear
        auto const& surface = asSurface(newNode);

        auto [child, nextUpdate] = updatedChild(oldTree, newTree, nullptr,
                surface.childNode_, animationOptions, time);

        if (!child)
            return { newNode, std::nullopt };

        return {
            std::make_shared<SurfaceNode>(
                    surface.getObb(),
                    surface.animationOptions_,
                    surface.shape_,
                    surface.fill_,
                    surface.shadow_,
                    std::move(child)
                    ),
            nextUpdate
        };
    }

    if (!newNode)
    {
        // Disappear
        auto const& surface = asSurface(oldNode);

        auto [child, nextUpdate] = updatedChild(oldTree, newTree,
                surface.childNode_, nullptr, animationOptions, time);

        if (!child)
            return { nullptr, std::nullopt };

        return {
            std::make_shared<SurfaceNode>(
                    surface.getObb(),
                    surface.animationOptions_,
                    surface.shape_,
                    surface.fill_,
                    surface.shadow_,
                    std::move(child)
                    ),
            nextUpdate
        };
    }

    assert(oldNode->getId() == newNode->getId());

    auto const& oldSurface = asSurface(oldNode);
    auto const& newSurface = asSurface(newNode);

    auto options = newSurface.animationOptions_
        ? newSurface.animationOptions_
        : animationOptions
        ;

    auto [child, nextUpdate] = updatedChild(oldTree, newTree,
            oldSurface.childNode_, newSurface.childNode_, animationOptions,
            time);

    return {
        std::make_shared<SurfaceNode>(
                oldSurface.getObb().updated(newSurface.getObb(), options,
                    time),
                newSurface.animationOptions_,
                oldSurface.shape_.updated(newSurface.shape_, options, time),
                oldSurface.fill_.updated(newSurface.fill_, options, time),
                updatedShadow(oldSurface.shadow_, newSurface.shadow_,
                    options, time),
                std::move(child)
                ),
        nextUpdate
    };
}

std::pair<Drawing, bool> SurfaceNode::draw(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    auto local = getObbAt(time);
    auto obb = parentObb.getTransform() * local;

    Shape shape = shape_(time, context, local.getSize());
    Drawing result = context.drawing();

    if (shadow_)
    {
        float spread = shadow_->spread.getValue(time);
        Brush brush = shadow_->brush.getValue(time);

        result.addShadow(Drawing::ShapeElement{
                Shape(shape).translate(shadow_->offset.getValue(time)),
                brush,
                spread > 0.0f
                    ? std::make_optional(Pen(brush, 2.0f * spread))
                    : std::nullopt
                });
    }

    if (auto fill = fill_.getValue(time))
        result += Drawing::ShapeElement{ std::move(shape), std::move(fill),
            std::nullopt };

    result = std::move(result).transform(obb.getTransform());

    bool cont = !getObb().hasAnimationEnded(time)
        || !shape_.hasAnimationEnded(time)
        || !fill_.hasAnimationEnded(time)
        || !hasShadowAnimationEnded(shadow_, time)
        ;

    if (childNode_)
    {
        auto [childDrawing, childCont] = childNode_->draw(context, obb, time);
        result += childDrawing;
        cont = cont || childCont;
    }

    return std::make_pair(std::move(result), cont);
}

SnapshotNode SurfaceNode::snapshot(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    auto result = makeSnapshotNode("SurfaceNode", *this, parentObb, time);

    if (childNode_)
        result.children.push_back(childNode_->snapshot(context, result.obb,
                    time));

    return result;
}

std::type_index SurfaceNode::getType() const
{
    return typeid(SurfaceNode);
}

std::shared_ptr<RenderTreeNode> SurfaceNode::clone() const
{
    return std::make_shared<SurfaceNode>(*this);
}

std::optional<AnimationOptions> const& SurfaceNode::getAnimationOptions() const
{
    return animationOptions_;
}

ShapeFunction const& SurfaceNode::getShape() const
{
    return shape_;
}

Animated<std::optional<Brush>> const& SurfaceNode::getFill() const
{
    return fill_;
}

std::optional<SurfaceShadow> const& SurfaceNode::getShadow() const
{
    return shadow_;
}

std::shared_ptr<RenderTreeNode> const& SurfaceNode::getChild() const
{
    return childNode_;
}

} // namespace avg
