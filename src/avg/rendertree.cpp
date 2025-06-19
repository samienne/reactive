#include "rendertree.h"

#include "obb.h"
#include "pathbuilder.h"
#include "transform.h"

#include <tracy/Tracy.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <iostream>
#include <optional>
#include <typeindex>

namespace avg
{

std::atomic<uint64_t> UniqueId::nextValue_ = 1;


UniqueId::UniqueId() :
    value_(nextValue_.fetch_add(1, std::memory_order_relaxed))
{
}

bool UniqueId::operator==(UniqueId const& id) const
{
    return value_ == id.value_;
}

bool UniqueId::operator!=(UniqueId const& id) const
{
    return value_ != id.value_;
}

bool UniqueId::operator<(UniqueId const& id) const
{
    return value_ < id.value_;
}

bool UniqueId::operator>(UniqueId const& id) const
{
    return value_ > id.value_;
}

std::ostream& operator<<(std::ostream& stream, UniqueId const& id)
{
    return stream << "id(" << id.value_ << ")";
}

std::optional<std::chrono::milliseconds> earlier(
        std::optional<std::chrono::milliseconds> t1,
        std::optional<std::chrono::milliseconds> t2
        )
{
    if (t1.has_value() && t2.has_value())
        return std::min(*t1, *t2);
    else if (t1.has_value())
        return t1;
    else
        return t2;
}

RenderTreeNode::RenderTreeNode(std::optional<UniqueId> id, Animated<Obb> obb) :
    id_(id),
    obb_(std::move(obb))
{
}

std::optional<UniqueId> const& RenderTreeNode::getId() const
{
    return id_;
}

Obb RenderTreeNode::getObbAt(std::chrono::milliseconds time) const
{
    return obb_.getValue(time);
}

Animated<Obb> const& RenderTreeNode::getObb() const
{
    return obb_;
}

Obb RenderTreeNode::getFinalObb() const
{
    return obb_.getFinalValue();
}

void RenderTreeNode::transform(Transform const& transform)
{
    /*
    obb_ = Animated<Obb>(
            transform * obb_.getInitialValue(),
            transform * obb_.getFinalValue(),
            obb_.getCurve(),
            obb_.getBeginTime(),
            obb_.getDuration()
            );
    */

    std::vector<Animated<Obb>::KeyFrame> keyFrames = obb_.getKeyFrames();
    for (auto& keyFrame : keyFrames)
        keyFrame.target = transform * keyFrame.target;

    obb_ = Animated<Obb>(
            transform * obb_.getInitialValue(),
            obb_.getBeginTime(),
            std::move(keyFrames)
            );
}

ContainerNode::ContainerNode(
        Animated<Obb> obb,
        std::vector<Child> children
        ) :
    RenderTreeNode(std::nullopt, obb),
    children_(std::move(children))
{
}

void ContainerNode::addChild(std::shared_ptr<RenderTreeNode> node)
{
    if (node)
        children_.push_back(Child { std::move(node), true });
}

void ContainerNode::addChildBehind(std::shared_ptr<RenderTreeNode> node)
{
    if (node)
    {
        children_.insert(
                children_.begin(),
                Child { std::move(node), true }
                );
    }
}

UpdateResult ContainerNode::update(
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
        return {
            newNode,
            std::nullopt
        };
    }
    else if (oldNode && !newNode)
    {
        // Disappear
        return {
            nullptr,
            std::nullopt
        };
    }

    assert(oldNode->getId() == newNode->getId());

    auto const& oldContainer = reinterpret_cast<ContainerNode const&>(*oldNode);
    auto const& newContainer = reinterpret_cast<ContainerNode const&>(*newNode);

    std::vector<Child> nodes;

    std::optional<std::chrono::milliseconds> nextUpdate;

    auto i = oldContainer.children_.begin();
    auto j = newContainer.children_.begin();

    while (i != oldContainer.children_.end() || j != newContainer.children_.end())
    {
        if (i != oldContainer.children_.end())
        {
            auto l = (i->active && i->node->getId().has_value())
                ? std::find_if(newContainer.children_.begin(), newContainer.children_.end(),
                    [&](auto const& child)
                    {
                        return i->node->getId() == child.node->getId();
                    })
                : newContainer.children_.end()
                ;

            bool oldNodeWasRemoved =
                (i->node->getId().has_value() && l == newContainer.children_.end())
                || (!i->node->getId().has_value() && j == newContainer.children_.end())
                ;

            if (!i->active || oldNodeWasRemoved)
            {
                auto [newChild, nextChildUpdate] = i->node->update(
                        oldTree,
                        newTree,
                        i->node,
                        nullptr,
                        animationOptions,
                        time
                        );

                if (newChild)
                {
                    nextUpdate = earlier(nextUpdate, nextChildUpdate);
                    nodes.emplace_back(std::move(newChild), false);
                }

                ++i;
                continue;
            }

            if (l != newContainer.children_.end())
            {
                ++i;
                continue;
            }
        }

        if (j != newContainer.children_.end())
        {
            auto k = j->node->getId().has_value()
                ? std::find_if(oldContainer.children_.begin(), oldContainer.children_.end(),
                        [&](auto const& child)
                        {
                            return j->node->getId() == child.node->getId();
                        })
                : oldContainer.children_.end();

            bool newNodeWasInserted =
                (!j->node->getId().has_value() && i == oldContainer.children_.end())
                || (j->node->getId().has_value()
                        && k == oldContainer.children_.end());

            if (newNodeWasInserted)
            {
                auto [newChild, nextChildUpdate] = j->node->update(
                        oldTree,
                        newTree,
                        nullptr,
                        j->node,
                        animationOptions,
                        time
                        );

                if (newChild)
                {
                    nextUpdate = earlier(nextUpdate, nextChildUpdate);
                    nodes.emplace_back(std::move(newChild), true);
                }

                ++j;
                continue;
            }

            if (j->node->getId().has_value() && k != oldContainer.children_.end())
            {
                auto [newChild, nextChildUpdate] = j->node->update(
                        oldTree,
                        newTree,
                        k->node,
                        j->node,
                        animationOptions,
                        time
                        );

                if (newChild)
                {
                    nextUpdate = earlier(nextUpdate, nextChildUpdate);
                    nodes.emplace_back(std::move(newChild), true);
                }

                ++j;
                continue;
            }
        }

        if (i != oldContainer.children_.end() && j != newContainer.children_.end())
        {
            auto [newChild, nextChildUpdate] = j->node->update(
                    oldTree,
                    newTree,
                    i->node,
                    j->node,
                    animationOptions,
                    time
                    );

            if (newChild)
            {
                nextUpdate = earlier(nextUpdate, nextChildUpdate);
                nodes.emplace_back(std::move(newChild), true);
            }

            ++i;
            ++j;
            continue;
        }

        assert(false);
    }

    return {
        std::make_shared<ContainerNode>(
                oldContainer.getObb().updated(newContainer.getObb(),
                    animationOptions, time),
                std::move(nodes)
                ),
        nextUpdate
    };
}

std::pair<Drawing, bool> ContainerNode::draw(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time) const
{
    Drawing result = context.drawing();

    bool cont = !getObb().hasAnimationEnded(time);

    auto obb = parentObb.getTransform() * getObb().getValue(time);

    for (auto const& child : children_)
    {
        auto [drawing, childCont] = child.node->draw(context, obb, time);
        cont = cont || childCont;
        result += std::move(drawing);
    }

    return std::make_pair(
            std::move(result),
            cont
            );
}

std::type_index ContainerNode::getType() const
{
    return typeid(ContainerNode);
}

std::shared_ptr<RenderTreeNode> ContainerNode::clone() const
{
    return std::make_shared<ContainerNode>(*this);
}

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

RenderTree::RenderTree()
{
}

RenderTree::RenderTree(std::shared_ptr<RenderTreeNode> root) :
    root_(std::move(root))
{
}

std::pair<RenderTree, std::optional<std::chrono::milliseconds>> RenderTree::update(
        RenderTree&& tree,
        std::optional<AnimationOptions> const& animationOptions,
        std::chrono::milliseconds time
        ) &&
{
    if (!root_)
    {
        return {
            std::move(tree),
            std::nullopt
        };
    }

    auto newRoot = tree.root_;

    if (!newRoot) {
        newRoot = std::make_shared<ContainerNode>(
                root_->getFinalObb()
                );
    }

    auto [updatedNode, nextUpdate] = tree.root_->update(
            *this,
            tree,
            root_,
            newRoot,
            animationOptions,
            time
            );

    return {
        RenderTree(std::move(updatedNode)),
        nextUpdate
    };
}

std::pair<Drawing, bool> RenderTree::draw(
        DrawContext const& context,
        avg::Obb const& obb,
        std::chrono::milliseconds time) const
{
    ZoneScopedN("RenderTree::draw");

    if (!root_)
    {
        return std::make_pair(context.drawing(), false);
    }

    return root_->draw(context, obb, time);
}

RenderTree RenderTree::transform(Transform const& transform) &&
{
    if (root_)
    {
        if (root_.use_count() != 1)
            root_ = root_->clone();

        root_->transform(transform);
    }

    return *this;
}

std::shared_ptr<RenderTreeNode> const& RenderTree::getRoot() const
{
    return root_;
}

} // namespace avg

