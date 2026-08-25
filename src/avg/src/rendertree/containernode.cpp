#include "rendertree/containernode.h"

#include "rendertree/updateresult.h"

#include "obb.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <utility>
#include <vector>

namespace avg
{

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

SnapshotNode ContainerNode::snapshot(DrawContext const& context,
        avg::Obb const& parentObb,
        std::chrono::milliseconds time) const
{
    auto result = makeSnapshotNode("ContainerNode", *this, parentObb, time);

    for (auto const& child : children_)
    {
        if (!child.node)
            continue;

        auto childSnapshot = child.node->snapshot(context, result.obb, time);
        childSnapshot.leaving = childSnapshot.leaving || !child.active;

        result.children.push_back(std::move(childSnapshot));
    }

    return result;
}

std::type_index ContainerNode::getType() const
{
    return typeid(ContainerNode);
}

std::shared_ptr<RenderTreeNode> ContainerNode::clone() const
{
    return std::make_shared<ContainerNode>(*this);
}

} // namespace avg
