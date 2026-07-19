#include "rendertree/transitionnode.h"

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

TransitionNode::TransitionNode(
        Animated<Obb> obb,
        bool isActive,
        std::shared_ptr<RenderTreeNode> activeNode,
        std::shared_ptr<RenderTreeNode> transitionedNode
        ) :
    RenderTreeNode(
            std::nullopt,
            std::move(obb)
            ),
    activeNode_(std::move(activeNode)),
    transitionedNode_(std::move(transitionedNode)),
    isActive_(isActive)
{
}

UpdateResult TransitionNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        std::optional<AnimationOptions> const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    std::shared_ptr<RenderTreeNode> oldActive;
    std::shared_ptr<RenderTreeNode> newActive;
    std::shared_ptr<RenderTreeNode> newTransitioned;
    std::optional<Animated<avg::Obb>> newObb;
    std::optional<std::chrono::milliseconds> nextUpdate;
    std::optional<Transition> transition;

    bool isActive = true;

    if (!oldNode && newNode)
    {
        // Appear
        auto const& newTransition = reinterpret_cast<TransitionNode const&>(*newNode);

        oldActive = newTransition.transitionedNode_;
        newActive = newTransition.activeNode_;
        newTransitioned = newTransition.transitionedNode_;
        newObb = newNode->getObb();

        if (animationOptions)
        {
            nextUpdate = time + animationOptions->duration;
            transition = Transition { time, animationOptions->duration };
        }
    }
    else if (oldNode && !newNode)
    {
        // Disappear
        auto const& oldTransition = reinterpret_cast<TransitionNode const&>(*oldNode);

        if (oldTransition.transition_)
        {
            if (oldTransition.transition_
                    && (oldTransition.transition_->startTime
                        + oldTransition.transition_->duration) <= time)
            {
                return {
                    nullptr,
                    std::nullopt
                };
            }

            return {
                oldNode,
                oldTransition.transition_->startTime + oldTransition.transition_->duration
            };
        }

        oldActive = oldTransition.activeNode_;
        newActive = oldTransition.transitionedNode_;
        newTransitioned = oldTransition.transitionedNode_;
        newObb = oldNode->getObb();
        isActive = false;

        if (animationOptions)
        {
            nextUpdate = time + animationOptions->duration;
            transition = Transition { time, animationOptions->duration };
        }
    }
    else
    {
        assert(oldNode->getId() == newNode->getId());
        assert(oldNode && newNode);

        auto const& newTransition = reinterpret_cast<TransitionNode const&>(*newNode);
        auto const& oldTransition = reinterpret_cast<TransitionNode const&>(*oldNode);

        isActive = oldTransition.isActive_;
        oldActive = oldTransition.activeNode_;
        newActive = isActive
            ? newTransition.activeNode_
            : newTransition.transitionedNode_
            ;
        newTransitioned = newTransition.transitionedNode_;
        newObb = oldNode->getObb().updated(newNode->getObb(),
                animationOptions, time);

        transition = oldTransition.transition_;
    }

    assert(newActive && newTransitioned && oldActive);
    assert(oldActive->getId() == newActive->getId());

    auto [resultNode, nextChildUpdate] = newActive->update(
            oldTree,
            newTree,
            oldActive,
            newActive,
            animationOptions,
            time
            );

    auto result = std::make_shared<TransitionNode>(
            *newObb,
            isActive,
            std::move(resultNode),
            std::move(newTransitioned)
            );

    if (transition.has_value())
        nextUpdate = transition->startTime + transition->duration;

    if (nextUpdate < time)
    {
        transition = std::nullopt;
        nextUpdate = std::nullopt;
    }

    result->transition_ = transition;

    return {
        std::move(result),
        earlier(nextUpdate, nextChildUpdate)
    };
}

std::pair<Drawing, bool> TransitionNode::draw(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time
        ) const
{
    bool cont = !getObb().hasAnimationEnded(time);

    auto obb = parentObb.getTransform() * getObb().getValue(time);

    auto [drawing, childCont] = activeNode_->draw(context, obb, time);

    return std::make_pair(
            std::move(drawing),
            cont || childCont
            );
}

std::type_index TransitionNode::getType() const
{
    return typeid(TransitionNode);
}

std::shared_ptr<RenderTreeNode> TransitionNode::clone() const
{
    return std::make_shared<TransitionNode>(*this);
}

} // namespace avg
