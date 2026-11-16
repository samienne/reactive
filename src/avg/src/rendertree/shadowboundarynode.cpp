#include "rendertree/shadowboundarynode.h"

#include "rendertree/updateresult.h"

#include "builtinelements.h"

#include "obb.h"

#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <utility>

namespace avg
{

namespace
{
    ShadowBoundaryNode const* asBoundary(
            std::shared_ptr<RenderTreeNode> const& node)
    {
        if (!node || node->getType() != typeid(ShadowBoundaryNode))
            return nullptr;

        return static_cast<ShadowBoundaryNode const*>(node.get());
    }

    ShadowLight updatedLight(ShadowLight const& oldLight,
            ShadowLight const& newLight,
            std::optional<AnimationOptions> const& options,
            std::chrono::milliseconds time)
    {
        return ShadowLight{
            oldLight.offset.updated(newLight.offset, options, time),
            oldLight.radius.updated(newLight.radius, options, time),
            oldLight.colour.updated(newLight.colour, options, time)
        };
    }

    bool hasLightAnimationEnded(ShadowLight const& light,
            std::chrono::milliseconds time)
    {
        return light.offset.hasAnimationEnded(time)
            && light.radius.hasAnimationEnded(time)
            && light.colour.hasAnimationEnded(time);
    }
} // anonymous namespace

ShadowBoundaryNode::ShadowBoundaryNode(
        std::optional<AnimationOptions> animationOptions,
        ShadowLight light,
        Content content,
        std::shared_ptr<RenderTreeNode> childNode
        ) :
    ShadowBoundaryNode(Animated<Obb>(Obb()),
            std::move(animationOptions), std::move(light), content,
            std::move(childNode))
{
}

ShadowBoundaryNode::ShadowBoundaryNode(
        Animated<Obb> obb,
        std::optional<AnimationOptions> animationOptions,
        ShadowLight light,
        Content content,
        std::shared_ptr<RenderTreeNode> childNode
        ) :
    RenderTreeNode(std::nullopt, std::move(obb)),
    animationOptions_(std::move(animationOptions)),
    light_(std::move(light)),
    content_(content),
    childNode_(std::move(childNode))
{
}

UpdateResult ShadowBoundaryNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        std::optional<AnimationOptions> const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    auto const* oldBoundary = asBoundary(oldNode);
    auto const* newBoundary = asBoundary(newNode);

    if (!oldBoundary && !newBoundary)
        return { nullptr, std::nullopt };

    if (!oldBoundary || !newBoundary)
    {
        auto const& node = newBoundary ? *newBoundary : *oldBoundary;

        if (!node.childNode_)
            return { newBoundary ? newNode : nullptr, std::nullopt };

        auto [child, nextUpdate] = node.childNode_->update(
                oldTree,
                newTree,
                newBoundary ? nullptr : node.childNode_,
                newBoundary ? node.childNode_ : nullptr,
                animationOptions,
                time
                );

        if (!child)
            return { newBoundary ? newNode : nullptr, std::nullopt };

        auto result = std::make_shared<ShadowBoundaryNode>(node);
        result->childNode_ = std::move(child);

        return { std::move(result), nextUpdate };
    }

    auto [child, nextUpdate] = updateNode(oldTree, newTree,
            oldBoundary->childNode_, newBoundary->childNode_,
            animationOptions, time);

    auto ownOptions = newBoundary->animationOptions_
        ? newBoundary->animationOptions_
        : animationOptions
        ;

    return {
        std::shared_ptr<ShadowBoundaryNode>(new ShadowBoundaryNode(
                    oldNode->getObb().updated(newNode->getObb(),
                        animationOptions, time),
                    newBoundary->animationOptions_,
                    updatedLight(oldBoundary->light_, newBoundary->light_,
                        ownOptions, time),
                    newBoundary->content_,
                    std::move(child)
                    )),
        nextUpdate
    };
}

std::pair<Drawing, bool> ShadowBoundaryNode::draw(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    auto obb = parentObb.getTransform() * getObbAt(time);

    auto [content, cont] = childNode_
        ? childNode_->draw(context, obb, time)
        : std::make_pair(context.drawing(), false);

    Outlines casters = content.takeOutlines();

    if (content_ == Content::scoped)
        content = std::move(content).scope();

    Drawing result = shadowOf(context.getResource(), std::move(casters),
            light_.offset.getValue(time),
            light_.radius.getValue(time),
            light_.colour.getValue(time));
    result += std::move(content);

    cont = cont
        || !getObb().hasAnimationEnded(time)
        || !hasLightAnimationEnded(light_, time)
        ;

    return { std::move(result), cont };
}

SnapshotNode ShadowBoundaryNode::snapshot(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    auto result = makeSnapshotNode("ShadowBoundaryNode", *this, parentObb,
            time);

    if (childNode_)
        result.children.push_back(childNode_->snapshot(context, result.obb,
                    time));

    return result;
}

std::type_index ShadowBoundaryNode::getType() const
{
    return typeid(ShadowBoundaryNode);
}

std::shared_ptr<RenderTreeNode> ShadowBoundaryNode::clone() const
{
    return std::make_shared<ShadowBoundaryNode>(*this);
}

ShadowLight const& ShadowBoundaryNode::getLight() const
{
    return light_;
}

ShadowBoundaryNode::Content ShadowBoundaryNode::getContent() const
{
    return content_;
}

std::shared_ptr<RenderTreeNode> const& ShadowBoundaryNode::getChild() const
{
    return childNode_;
}

} // namespace avg
