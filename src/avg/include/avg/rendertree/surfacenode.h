#pragma once

#include "rendertreenode.h"

#include "avg/animated.h"
#include "avg/brush.h"
#include "avg/color.h"
#include "avg/drawing.h"
#include "avg/drawcontext.h"
#include "avg/shapefunction.h"
#include "avg/vector.h"
#include "avg/avgvisibility.h"

#include <optional>
#include <chrono>
#include <memory>
#include <typeindex>
#include <utility>

namespace avg
{
    /**
     * @brief A filled shape with a child subtree on it and an optional
     * backdrop effect behind its fill.
     *
     * The shape is produced for the node's size, so
     * `SurfaceNode(obb, options, roundedRect, Brush(white), std::nullopt,
     * child)` paints a white card that shows child on top; a brush in place of the std::nullopt paints the
     * shape before the fill, so an effect brush under a translucent fill
     * shows frosted glass. The effect brush animates through lerp(): two
     * brushes of the same kind (isSameKind) interpolate, and a change of
     * kind switches at once. An opaque colour fill makes the shape the
     * drawing's outline; a surface with an effect has none, however it is
     * filled. A surface adds no backdrop scope; stack
     * surfaces with a stacking ContainerNode to layer them. The animation
     * options apply to the surface's own obb, shape, fill and effect, not
     * to child.
     */
    class AVG_EXPORT SurfaceNode : public RenderTreeNode
    {
    public:
        SurfaceNode(
                Animated<Obb> obb,
                std::optional<AnimationOptions> animationOptions,
                ShapeFunction shape,
                Animated<std::optional<Brush>> fill,
                std::optional<Animated<Brush>> effect,
                std::shared_ptr<RenderTreeNode> childNode
                );

        SurfaceNode(SurfaceNode const&) = default;
        SurfaceNode& operator=(SurfaceNode const&) = default;

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

        std::optional<AnimationOptions> const& getAnimationOptions() const;
        ShapeFunction const& getShape() const;
        Animated<std::optional<Brush>> const& getFill() const;
        std::optional<Animated<Brush>> const& getEffect() const;
        std::shared_ptr<RenderTreeNode> const& getChild() const;

    private:
        std::optional<AnimationOptions> animationOptions_;
        ShapeFunction shape_;
        Animated<std::optional<Brush>> fill_;
        std::optional<Animated<Brush>> effect_;
        std::shared_ptr<RenderTreeNode> childNode_;
    };
} // namespace avg
