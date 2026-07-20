#pragma once

#include "rendertreenode.h"

#include "avg/animated.h"
#include "avg/drawing.h"
#include "avg/drawcontext.h"
#include "avg/avgvisibility.h"

#include <optional>
#include <chrono>
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

namespace avg
{
    class AVG_EXPORT ContainerNode : public RenderTreeNode
    {
        struct Child
        {
            inline Child(
                std::shared_ptr<RenderTreeNode> node,
                bool active
                ) :
                node(std::move(node)),
                active(active)
            {
            }

            std::shared_ptr<RenderTreeNode> node;
            bool active = false;
        };

    public:
        ContainerNode(
                Animated<Obb> obb,
                std::vector<Child> children = {}
                );

        ContainerNode(ContainerNode const&) = default;

        void addChild(std::shared_ptr<RenderTreeNode> node);
        void addChildBehind(std::shared_ptr<RenderTreeNode> node);

        UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& parentObb,
                std::chrono::milliseconds time) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        std::vector<Child> children_;
    };
} // namespace avg
