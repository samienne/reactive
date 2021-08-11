#include "rendertree.h"

#include "obb.h"
#include "pathbuilder.h"
#include "transform.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <iostream>

namespace avg
{

std::atomic<uint64_t> UniqueId::nextValue_ = 1;

float linearCurve(float f)
{
    return f;
}

float lerp(float a, float b, float t)
{
    return a + (b-a) * t;
}

Vector2f lerp(Vector2f a, Vector2f b, float t)
{
    return a + (b-a) * t;
}

Rect lerp(Rect a, Rect b, float t)
{
    return Rect(
            lerp(a.getBottomLeft(), b.getBottomLeft(), t),
            lerp(a.getSize(), b.getSize(), t)
        );
}

Transform lerp(Transform const a, Transform const& b, float t)
{
    return Transform(
            lerp(a.getTranslation(), b.getTranslation(), t),
            lerp(a.getScale(), b.getScale(), t),
            lerp(a.getRotation(), b.getRotation(), t)
            );
}

Obb lerp(Obb const& a, Obb const& b, float t)
{
    return Obb(
            lerp(a.getSize(), b.getSize(), t),
            lerp(a.getTransform(), b.getTransform(), t)
            );
}

Color lerp(Color const& a, Color const& b, float t)
{
    return Color(
            lerp(a.getRed(), b.getRed(), t),
            lerp(a.getGreen(), b.getGreen(), t),
            lerp(a.getBlue(), b.getBlue(), t),
            lerp(a.getAlpha(), b.getAlpha(), t)
            );
}

Brush lerp(Brush const& a, Brush const& b, float t)
{
    return Brush(lerp(a.getColor(), b.getColor(), t));
}

Pen lerp(Pen const& a, Pen const& b, float t)
{
    return Pen(
            lerp(a.getBrush(), b.getBrush(), t),
            lerp(a.getWidth(), b.getWidth(), t),
            b.getJoinType(),
            b.getEndType()
            );
}

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

RenderTreeNode::RenderTreeNode(UniqueId id, Animated<Obb> obb,
        TransitionOptions const& transition) :
    id_(id),
    geometryId_(id),
    transitionOptions_(transition),
    obb_(std::move(obb))
{
}

UniqueId const& RenderTreeNode::getId() const
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

TransitionOptions const& RenderTreeNode::getTransitionOptions() const
{
    return transitionOptions_;
}

void RenderTreeNode::transform(Transform const& transform)
{
    obb_ = Animated<Obb>(
            transform * obb_.getInitialValue(),
            transform * obb_.getFinalValue(),
            obb_.getCurve(),
            obb_.getBeginTime(),
            obb_.getDuration()
            );
}

ContainerNode::ContainerNode(UniqueId id, Animated<Obb> obb,
        TransitionOptions transitionOptions,
        std::vector<std::shared_ptr<RenderTreeNode>> children) :
    RenderTreeNode(id, obb, transitionOptions),
    children_(std::move(children))
{
}

void ContainerNode::addChild(std::shared_ptr<RenderTreeNode> node)
{
    if (node)
        children_.push_back(std::move(node));
}

void ContainerNode::addChildBehind(std::shared_ptr<RenderTreeNode> node)
{
    if (node)
        children_.insert(children_.begin(), std::move(node));
}

UpdateResult ContainerNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        AnimationOptions const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    if (!oldNode && newNode)
    {
        // Appear
        std::cout << "Appear" << newNode->getId() << std::endl;
        return {
            newNode,
            std::nullopt
        };
    }
    else if (oldNode && !newNode)
    {
        // Disappear
        std::cout << "Disappear" << oldNode->getId() << std::endl;
        return {
            nullptr,
            std::nullopt
        };
    }

    assert(oldNode->getId() == newNode->getId());

    auto const& oldContainer = reinterpret_cast<ContainerNode const&>(*oldNode);
    auto const& newContainer = reinterpret_cast<ContainerNode const&>(*newNode);

    std::vector<std::shared_ptr<RenderTreeNode>> nodes;

    std::optional<std::chrono::milliseconds> nextUpdate;

    for (auto const& child : newContainer.children_)
    {
        auto i = std::find_if(oldContainer.children_.begin(),
                oldContainer.children_.end(),
                [&](std::shared_ptr<RenderTreeNode> const& node)
                {
                    return node->getId() == child->getId();
                });

        std::shared_ptr<RenderTreeNode> oldChild = nullptr;
        if (i != oldContainer.children_.end())
            oldChild = *i;

        auto [newChild, nextChildUpdate] = child->update(
                oldTree,
                newTree,
                oldChild,
                child,
                animationOptions,
                time
                );

        nextUpdate = earlier(nextUpdate, nextChildUpdate);

        if (newChild)
            nodes.push_back(std::move(newChild));
    }

    for (auto const& oldChild : oldContainer.children_)
    {
        auto i = std::find_if(newContainer.children_.begin(),
                newContainer.children_.end(),
                [&](std::shared_ptr<RenderTreeNode> const& node)
                {
                    return node->getId() == oldChild->getId();
                });

        if (i == newContainer.children_.end())
        {
            auto [newChild, nextChildUpdate] = oldChild->update(
                    oldTree,
                    newTree,
                    oldChild,
                    nullptr,
                    animationOptions,
                    time
                    );

            nextUpdate = earlier(nextUpdate, nextChildUpdate);

            if (newChild)
                nodes.push_back(std::move(newChild));;
        }
    }

    return {
        std::make_shared<ContainerNode>(
                newNode->getId(),
                oldContainer.getObb().updated(newContainer.getObb(),
                    animationOptions, time),
                newNode->getTransitionOptions(),
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
        auto [drawing, childCont] = child->draw(context, obb, time);
        cont = cont || childCont;
        result += std::move(drawing);
    }

    return std::make_pair(
            std::move(result),
            cont
            );
}

RectNode::RectNode(
        UniqueId id,
        Animated<Obb> obb,
        TransitionOptions transitionOptions,
        Animated<float> radius,
        Animated<btl::option<Brush>> brush,
        Animated<btl::option<Pen>> pen
        ) :
    ShapeNode(id, std::move(obb), std::move(transitionOptions),
            RectNode::drawRect, std::move(radius), std::move(brush),
            std::move(pen))
{
}

Drawing RectNode::drawRect(
        DrawContext const& context,
        Vector2f size,
        float radius,
        btl::option<Brush> const& brush,
        btl::option<Pen> const& pen
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

    return context.drawing()
        + Shape(path, brush, pen)
        ;
}

TransitionNode::TransitionNode(
        UniqueId id,
        Animated<Obb> obb,
        TransitionOptions transitionOptions,
        bool isActive,
        std::shared_ptr<RenderTreeNode> activeNode,
        std::shared_ptr<RenderTreeNode> transitionedNode
        ) :
    RenderTreeNode(
            id,
            std::move(obb),
            transitionOptions
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
        AnimationOptions const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    std::shared_ptr<RenderTreeNode> oldActive;
    std::shared_ptr<RenderTreeNode> newActive;
    std::shared_ptr<RenderTreeNode> newTransitioned;
    std::optional<Animated<avg::Obb>> newObb;
    std::optional<UniqueId> transitionId;
    std::optional<std::chrono::milliseconds> nextUpdate;
    std::optional<Transition> transition;

    bool isActive = true;

    if (!oldNode && newNode)
    {
        // Appear
        auto const& newTransition = reinterpret_cast<TransitionNode const&>(*newNode);
        std::cout << "appear transition: " << newNode->getId() << std::endl;

        oldActive = newTransition.transitionedNode_;
        newActive = newTransition.activeNode_;
        newTransitioned = newTransition.transitionedNode_;
        newObb = newNode->getObb();
        transitionId = newNode->getId();
    }
    else if (oldNode && !newNode)
    {
        // Disappear
        std::cout << "disappear transition: " << oldNode->getId() << std::endl;
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
        transitionId = oldNode->getId();
        nextUpdate = time + animationOptions.duration;
        transition = Transition { time, animationOptions.duration };
    }
    else
    {
        assert(oldNode->getId() == newNode->getId());
        assert(oldNode && newNode);

        auto const& newTransition = reinterpret_cast<TransitionNode const&>(*newNode);
        auto const& oldTransition = reinterpret_cast<TransitionNode const&>(*oldNode);

        bool isActive = oldTransition.isActive_;
        oldActive = oldTransition.activeNode_;
        newActive = isActive
            ? newTransition.activeNode_
            : newTransition.transitionedNode_
            ;
        newTransitioned = newTransition.transitionedNode_;
        newObb = oldNode->getObb().updated(newNode->getObb(),
                animationOptions, time);
        transitionId = newNode->getId();

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
            *transitionId,
            *newObb,
            newActive->getTransitionOptions(),
            isActive,
            std::move(resultNode),
            std::move(newTransitioned)
            );

    result->transition_ = transition;

    if (transition.has_value())
        nextUpdate = transition->startTime + transition->duration;

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

RenderTree::RenderTree()
{
}

RenderTree::RenderTree(std::shared_ptr<RenderTreeNode> root) :
    root_(std::move(root))
{
}

std::pair<RenderTree, std::optional<std::chrono::milliseconds>> RenderTree::update(
        RenderTree&& tree,
        AnimationOptions const& animationOptions,
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
                root_->getId(),
                root_->getFinalObb(),
                root_->getTransitionOptions(),
                std::vector<std::shared_ptr<RenderTreeNode>>()
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
    if (!root_)
    {
        return std::make_pair(context.drawing(), false);
    }

    return root_->draw(context, obb, time);
}

RenderTree RenderTree::transform(Transform const& transform) &&
{
    if (root_)
        root_->transform(transform);

    return *this;
}

std::shared_ptr<RenderTreeNode> const& RenderTree::getRoot() const
{
    return root_;
}

} // namespace avg

