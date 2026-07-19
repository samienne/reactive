#include "rendertree/idnode.h"

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

IdNode::IdNode(
        UniqueId id,
        Animated<Obb> obb,
        std::shared_ptr<RenderTreeNode> childNode
        ) :
    RenderTreeNode(std::move(id), obb),
    childNode_(std::move(childNode))
{
}

UpdateResult IdNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        std::optional<AnimationOptions> const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    if (!oldNode && newNode)
    {
        // Appear
        auto const& newId = reinterpret_cast<IdNode const&>(*newNode);

        auto [newChild, nextChildUpdate] = newId.childNode_->update(
                oldTree,
                newTree,
                nullptr,
                newId.childNode_,
                animationOptions,
                time
                );

        if (newChild)
        {
            return {
                std::make_shared<IdNode>(
                        *newNode->getId(),
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
    else if (oldNode && !newNode)
    {
        // Disappear
        auto const& oldId = reinterpret_cast<IdNode const&>(*oldNode);
        auto [newChild, nextChildUpdate] = oldId.childNode_->update(
                oldTree,
                newTree,
                oldId.childNode_,
                nullptr,
                animationOptions,
                time
                );

        if (newChild)
        {
            return {
                std::make_shared<IdNode>(
                        *oldNode->getId(),
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

    auto const& oldId = reinterpret_cast<IdNode const&>(*oldNode);
    auto const& newId = reinterpret_cast<IdNode const&>(*newNode);

    std::optional<std::chrono::milliseconds> nextUpdate;

    if (oldId.childNode_->getId() != newId.childNode_->getId())
    {
        auto [newChild, nextNewUpdate] = newNode->update(
                oldTree,
                newTree,
                nullptr,
                newId.childNode_,
                animationOptions,
                time
                );

        //nextUpdate = earlier(nextOldUpdate, nextNewUpdate);
        nextUpdate = nextNewUpdate;

        return {
            std::make_shared<IdNode>(
                    *oldId.getId(),
                    oldId.getObb().updated(
                        newId.getObb(),
                        animationOptions,
                        time
                        ),
                    std::move(newChild)
                    ),
            nextUpdate
        };
    }

    auto [newChild, nextChildUpdate] = oldId.childNode_->update(
            oldTree,
            newTree,
            oldId.childNode_,
            newId.childNode_,
            animationOptions,
            time
            );

    return {
        std::make_shared<IdNode>(
                *oldId.getId(),
                oldNode->getObb().updated(newNode->getObb(),
                    animationOptions, time),
                std::move(newChild)
                ),
        earlier(nextUpdate, nextChildUpdate)
    };
}

std::pair<Drawing, bool> IdNode::draw(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    auto obb = parentObb.getTransform() * getObbAt(time);

    auto [drawing, childCont] = childNode_->draw(
            context,
            obb,
            time
            );

    return {
        std::move(drawing),
        !getObb().hasAnimationEnded(time) || childCont
    };
}

std::type_index IdNode::getType() const
{
    return typeid(IdNode);
}

std::shared_ptr<RenderTreeNode> IdNode::clone() const
{
    return std::make_shared<IdNode>(*this);
}

} // namespace avg
