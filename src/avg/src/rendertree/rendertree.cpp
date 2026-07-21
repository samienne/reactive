#include "rendertree/rendertree.h"

#include "rendertree/rendertreenode.h"
#include "rendertree/containernode.h"

#include "obb.h"

#include <tracy/Tracy.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

namespace avg
{

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

Snapshot RenderTree::snapshot(
        DrawContext const& drawContext,
        avg::Obb const& obb,
        std::chrono::milliseconds time) const
{
    ZoneScopedN("RenderTree::snapshot");

    Snapshot result;

    result.time = time;
    result.obb = obb;

    if (root_)
        result.root = root_->snapshot(drawContext, obb, time);

    return result;
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
