#include "rendertree/outlinenode.h"

#include "rendertree/updateresult.h"

#include "obb.h"
#include "shape.h"

#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <utility>

namespace avg
{

namespace
{
    OutlineNode const* asOutline(std::shared_ptr<RenderTreeNode> const& node)
    {
        if (!node || node->getType() != typeid(OutlineNode))
            return nullptr;

        return static_cast<OutlineNode const*>(node.get());
    }

    std::optional<ShapeFunction> updatedOutline(
            std::optional<ShapeFunction> const& oldOutline,
            std::optional<ShapeFunction> const& newOutline,
            std::optional<AnimationOptions> const& options,
            std::chrono::milliseconds time)
    {
        if (!oldOutline || !newOutline)
            return newOutline;

        return oldOutline->updated(*newOutline, options, time);
    }
} // anonymous namespace

OutlineNode::OutlineNode(
        Animated<Obb> area,
        std::optional<AnimationOptions> animationOptions,
        Mode mode,
        std::optional<ShapeFunction> outline,
        std::shared_ptr<RenderTreeNode> childNode
        ) :
    OutlineNode(Animated<Obb>(Obb()), std::move(area),
            std::move(animationOptions), mode, std::move(outline),
            std::move(childNode))
{
}

OutlineNode::OutlineNode(
        Animated<Obb> obb,
        Animated<Obb> area,
        std::optional<AnimationOptions> animationOptions,
        Mode mode,
        std::optional<ShapeFunction> outline,
        std::shared_ptr<RenderTreeNode> childNode
        ) :
    RenderTreeNode(std::nullopt, std::move(obb)),
    area_(std::move(area)),
    animationOptions_(std::move(animationOptions)),
    mode_(mode),
    outline_(std::move(outline)),
    childNode_(std::move(childNode))
{
}

UpdateResult OutlineNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        std::optional<AnimationOptions> const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    // An old node of another kind is not paired: the new node appears.
    auto const* oldOutline = asOutline(oldNode);
    auto const* newOutline = asOutline(newNode);

    if (!oldOutline && !newOutline)
        return { nullptr, std::nullopt };

    if (!oldOutline || !newOutline)
    {
        auto const& node = newOutline ? *newOutline : *oldOutline;

        if (!node.childNode_)
            return { newOutline ? newNode : nullptr, std::nullopt };

        auto [child, nextUpdate] = node.childNode_->update(
                oldTree,
                newTree,
                newOutline ? nullptr : node.childNode_,
                newOutline ? node.childNode_ : nullptr,
                animationOptions,
                time
                );

        if (!child)
            return { newOutline ? newNode : nullptr, std::nullopt };

        auto result = std::make_shared<OutlineNode>(node);
        result->childNode_ = std::move(child);

        return { std::move(result), nextUpdate };
    }

    auto const& oldChild = oldOutline->childNode_;
    auto const& newChild = newOutline->childNode_;

    auto [child, nextUpdate] = updateNode(oldTree, newTree, oldChild,
            newChild, animationOptions, time);

    auto areaOptions = newOutline->animationOptions_
        ? newOutline->animationOptions_
        : animationOptions
        ;

    return {
        std::shared_ptr<OutlineNode>(new OutlineNode(
                    oldNode->getObb().updated(newNode->getObb(),
                        animationOptions, time),
                    oldOutline->area_.updated(newOutline->area_, areaOptions,
                        time),
                    newOutline->animationOptions_,
                    newOutline->mode_,
                    updatedOutline(oldOutline->outline_,
                        newOutline->outline_, areaOptions, time),
                    std::move(child)
                    )),
        nextUpdate
    };
}

std::pair<Drawing, bool> OutlineNode::draw(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    auto obb = parentObb.getTransform() * getObbAt(time);

    auto [drawing, cont] = childNode_
        ? childNode_->draw(context, obb, time)
        : std::make_pair(context.drawing(), false);

    if (mode_ == Mode::replace)
        drawing = std::move(drawing).clearOutlines();

    if (outline_)
    {
        auto area = area_.getValue(time);

        drawing.addOutline((*outline_)(time, context, area.getSize())
                .transform(obb.getTransform() * area.getTransform()));
    }

    cont = cont
        || !getObb().hasAnimationEnded(time)
        || !area_.hasAnimationEnded(time)
        || (outline_ && !outline_->hasAnimationEnded(time))
        ;

    return { std::move(drawing), cont };
}

SnapshotNode OutlineNode::snapshot(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    auto result = makeSnapshotNode("OutlineNode", *this, parentObb, time);

    if (childNode_)
        result.children.push_back(childNode_->snapshot(context, result.obb,
                    time));

    return result;
}

std::type_index OutlineNode::getType() const
{
    return typeid(OutlineNode);
}

std::shared_ptr<RenderTreeNode> OutlineNode::clone() const
{
    return std::make_shared<OutlineNode>(*this);
}

} // namespace avg
