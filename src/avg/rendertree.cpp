#include "rendertree.h"

#include "pathbuilder.h"

#include <atomic>
#include <chrono>

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

Obb RenderTreeNode::getObb(std::chrono::milliseconds time) const
{
    return obb_.getValue(time);
}

Obb RenderTreeNode::getFinalObb() const
{
    return obb_.getFinalValue();
}

TransitionOptions const& RenderTreeNode::getTransitionOptions() const
{
    return transitionOptions_;
}

ContainerNode::ContainerNode(UniqueId id, Animated<Obb> obb,
        TransitionOptions transitionOptions,
        std::vector<std::shared_ptr<RenderTreeNode>> children) :
    RenderTreeNode(id, obb, transitionOptions),
    children_(std::move(children))
{
}

std::shared_ptr<RenderTreeNode> ContainerNode::update(
        RenderTree const& oldTree,
        RenderTree const& newTree,
        std::shared_ptr<RenderTreeNode> const& oldNode,
        std::shared_ptr<RenderTreeNode> const& newNode,
        AnimationOptions const& animationOptions,
        std::chrono::milliseconds time
        ) const
{
    assert(oldNode->getId() == newNode->getId());

    if (!oldNode && newNode)
    {
        // Appear
        return newNode;
    }
    else if (oldNode && !newNode)
    {
        // Disappear
        return nullptr;
    }

    auto const& oldContainer = reinterpret_cast<ContainerNode const&>(*oldNode);
    auto const& newContainer = reinterpret_cast<ContainerNode const&>(*newNode);

    std::vector<std::shared_ptr<RenderTreeNode>> nodes;

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

        auto newChild = child->update(
                oldTree,
                newTree,
                oldChild,
                child,
                animationOptions,
                time
                );

        if (newChild)
            nodes.push_back(std::move(newChild));
    }

    return std::make_shared<ContainerNode>(
            newNode->getId(),
            Animated<Obb>(newNode->getObb(time)),
            newNode->getTransitionOptions(),
            std::move(nodes)
            );
}

Drawing ContainerNode::draw(pmr::memory_resource* memory,
        std::chrono::milliseconds time) const
{
    Drawing result(memory);

    for (auto const& child : children_)
    {
        result += child->draw(memory, time);
    }

    return result;
}

RectNode::RectNode(
        UniqueId id,
        Animated<Obb> obb,
        TransitionOptions transitionOptions,
        Animated<float> radius
        ) :
    ShapeNode<float>(id, std::move(obb), std::move(transitionOptions),
            RectNode::drawRect, std::move(radius))
{
}

Drawing RectNode::drawRect(
        pmr::memory_resource* memory,
        Vector2f size,
        float radius
        )
{
    auto path = PathBuilder(memory)
        .start(Vector2f(0.0f, 0.0f))
        .lineTo(0.0f, size[1])
        .lineTo(size[0], size[1])
        .lineTo(size[0], 0.0f)
        .close()
        .build();

    return Drawing(memory)
        + Shape(path, btl::none, btl::just(Pen(Brush(Color(0.0f, 1.0f, 0.0f)))))
        ;
}

RenderTree::RenderTree()
{
}

RenderTree::RenderTree(std::shared_ptr<RenderTreeNode> root) :
    root_(std::move(root))
{
}

RenderTree RenderTree::update(
        RenderTree&& tree,
        AnimationOptions const& animationOptions,
        std::chrono::milliseconds time
        ) &&
{
    if (!root_)
        return std::move(tree);

    auto newRoot = tree.root_;

    if (!newRoot) {
        newRoot = std::make_shared<ContainerNode>(
                root_->getId(),
                root_->getFinalObb(),
                root_->getTransitionOptions(),
                std::vector<std::shared_ptr<RenderTreeNode>>()
                );

    }


    newRoot = tree.root_->update(
            *this,
            tree,
            root_,
            newRoot,
            animationOptions,
            time
            );

    return RenderTree(std::move(newRoot));
}

Drawing RenderTree::draw(pmr::memory_resource* memory,
        std::chrono::milliseconds time) const
{
    if (!root_)
    {
        return Drawing(memory);
    }

    return root_->draw(memory, time);
}

} // namespace avg

