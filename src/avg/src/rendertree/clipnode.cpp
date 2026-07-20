#include "rendertree/clipnode.h"

#include "rendertree/updateresult.h"

#include "obb.h"

#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <utility>

namespace avg
{

ClipNode::ClipNode(
        Animated<Obb> obb,
        std::shared_ptr<RenderTreeNode> childNode
        ) :
    RenderTreeNode(std::nullopt, obb),
    childNode_(std::move(childNode))
{
}

UpdateResult ClipNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        std::optional<AnimationOptions> const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    auto const& newClip = reinterpret_cast<ClipNode const&>(*newNode);
    auto const& oldClip = reinterpret_cast<ClipNode const&>(*oldNode);
    bool const hasOldNode = oldNode && oldClip.childNode_;
    bool const hasNewNode = newNode && newClip.childNode_;

    if (!hasOldNode && !hasNewNode)
    {
        return {
            nullptr,
            std::nullopt
        };
    }

    if (!hasOldNode && hasNewNode)
    {
        // Appear

        auto [newChild, nextChildUpdate] = newClip.childNode_->update(
                oldTree,
                newTree,
                nullptr,
                newClip.childNode_,
                animationOptions,
                time
                );

        if (newChild)
        {
            return {
                std::make_shared<ClipNode>(
                        newNode->getObb(),
                        std::move(newChild)
                        ),
                nextChildUpdate
            };
        };

        return {
            newNode,
            std::nullopt
        };
    }
    else if (hasOldNode && !hasNewNode)
    {
        // Disappear
        auto [newChild, nextChildUpdate] = oldClip.childNode_->update(
                oldTree,
                newTree,
                oldClip.childNode_,
                nullptr,
                animationOptions,
                time
                );

        if (newChild)
        {
            return {
                std::make_shared<ClipNode>(
                        oldNode->getObb(),
                        std::move(newChild)
                        ),
                nextChildUpdate
            };
        };

        return {
            nullptr,
            std::nullopt
        };
    }

    assert(oldNode->getId() == newNode->getId());

    //auto const& oldClip = reinterpret_cast<ClipNode const&>(*oldNode);
    //auto const& newClip = reinterpret_cast<ClipNode const&>(*newNode);

    std::optional<std::chrono::milliseconds> nextUpdate;

    if (oldClip.childNode_->getId() != newClip.childNode_->getId())
    {
        auto [newChild, nextNewUpdate] = newNode->update(
                oldTree,
                newTree,
                nullptr,
                newClip.childNode_,
                animationOptions,
                time
                );

        //nextUpdate = earlier(nextOldUpdate, nextNewUpdate);
        nextUpdate = nextNewUpdate;

        return {
            std::make_shared<ClipNode>(
                    oldClip.getObb().updated(
                        newClip.getObb(),
                        animationOptions,
                        time
                        ),
                    std::move(newChild)
                    ),
            nextUpdate
        };
    }

    auto [newChild, nextChildUpdate] = oldClip.childNode_->update(
            oldTree,
            newTree,
            oldClip.childNode_,
            newClip.childNode_,
            animationOptions,
            time
            );

    return {
        std::make_shared<ClipNode>(
                oldNode->getObb().updated(newNode->getObb(),
                    animationOptions, time),
                std::move(newChild)
                ),
        earlier(nextUpdate, nextChildUpdate)
    };
}

std::pair<Drawing, bool> ClipNode::draw(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    if (!childNode_)
    {
        return std::make_pair(context.drawing(), false);
    }

    auto obb = parentObb.getTransform() * getObbAt(time);

    auto [drawing, childCont] = childNode_->draw(
            context,
            obb,
            time
            );

    return {
        std::move(drawing).clip(obb),
        !getObb().hasAnimationEnded(time) || childCont
    };
}

std::type_index ClipNode::getType() const
{
    return typeid(ClipNode);
}

std::shared_ptr<RenderTreeNode> ClipNode::clone() const
{
    return std::make_shared<ClipNode>(*this);
}

} // namespace avg
