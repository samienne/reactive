#pragma once

#include "rendertreenode.h"

#include "avg/animated.h"
#include "avg/animationoptions.h"
#include "avg/drawcontext.h"
#include "avg/drawing.h"
#include "avg/shapefunction.h"
#include "avg/avgvisibility.h"

#include <optional>
#include <chrono>
#include <memory>
#include <typeindex>
#include <utility>

namespace avg
{
    /**
     * @brief Draws its child and replaces or adds to the outlines of the
     * child's drawing.
     *
     * The outline is the shape function's value for area's size, placed by
     * area's transform in the node's space. With Mode::replace the child's outlines are dropped and a
     * std::nullopt outline leaves none; with Mode::add the outline joins
     * them. The node's own obb only carries placement and moves with the
     * options the parent passes; the animation options apply to area and
     * to the outline function's parameters.
     */
    class AVG_EXPORT OutlineNode : public RenderTreeNode
    {
    public:
        enum class Mode
        {
            replace,
            add
        };

        OutlineNode(
                Animated<Obb> area,
                std::optional<AnimationOptions> animationOptions,
                Mode mode,
                std::optional<ShapeFunction> outline,
                std::shared_ptr<RenderTreeNode> childNode
                );

        OutlineNode(OutlineNode const&) = default;
        OutlineNode& operator=(OutlineNode const&) = default;

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
                std::chrono::milliseconds time
                ) const override;

        SnapshotNode snapshot(DrawContext const& context,
                avg::Obb const& parentObb,
                std::chrono::milliseconds time
                ) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        OutlineNode(
                Animated<Obb> obb,
                Animated<Obb> area,
                std::optional<AnimationOptions> animationOptions,
                Mode mode,
                std::optional<ShapeFunction> outline,
                std::shared_ptr<RenderTreeNode> childNode
                );

        Animated<Obb> area_;
        std::optional<AnimationOptions> animationOptions_;
        Mode mode_;
        std::optional<ShapeFunction> outline_;
        std::shared_ptr<RenderTreeNode> childNode_;
    };
} // namespace avg
