#pragma once

#include "rendertreenode.h"

#include "avg/animated.h"
#include "avg/brush.h"
#include "avg/builtineffects.h"
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
#include <variant>

namespace avg
{
    /**
     * @brief The shadow a surface casts: its shape moved by offset, grown
     * outward by spread and filled with brush.
     */
    struct SurfaceShadow
    {
        Animated<Vector2f> offset;
        Animated<float> spread;
        Animated<Brush> brush;
    };

    /**
     * @brief Frosted glass behind a surface, as frostedGlass() makes it.
     */
    struct SurfaceFrostedGlass
    {
        Animated<FrostedGlassOptions> options;
    };

    /**
     * @brief The backdrop behind a surface tinted by colour, as tint()
     * makes it.
     */
    struct SurfaceTint
    {
        Animated<Color> colour;
        Animated<float> strength;
    };

    /**
     * @brief The backdrop effect a surface paints behind its fill.
     *
     *     SurfaceEffect glass = SurfaceFrostedGlass{ options };
     *
     * Its members animate as values, so a change of radius or tint never
     * changes what the surface draws, only how it looks. A value that an
     * animation curve carries out of its range is drawn clamped to it.
     */
    using SurfaceEffect = std::variant<SurfaceFrostedGlass, SurfaceTint>;

    /**
     * @brief A filled shape with a child subtree on it, an optional shadow
     * under it and an optional backdrop effect behind its fill.
     *
     * The shape is produced for the node's size, so
     * `SurfaceNode(obb, options, roundedRect, Brush(white), shadow,
     * std::nullopt, child)` paints a white card that casts the shadow and
     * shows child on top; a SurfaceEffect in place of the std::nullopt
     * draws the effect through the shape before the fill, so a translucent
     * fill shows frosted glass. The shadow is a pending shadow of the
     * drawing: the container holding the surface paints it under all of
     * its content, not under the surface alone. A surface adds no backdrop
     * scope; stack surfaces with a stacking ContainerNode to layer them.
     * The animation options apply to the surface's own obb, shape, fill,
     * shadow and effect, not to child.
     */
    class AVG_EXPORT SurfaceNode : public RenderTreeNode
    {
    public:
        SurfaceNode(
                Animated<Obb> obb,
                std::optional<AnimationOptions> animationOptions,
                ShapeFunction shape,
                Animated<std::optional<Brush>> fill,
                std::optional<SurfaceShadow> shadow,
                std::optional<SurfaceEffect> effect,
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
        std::optional<SurfaceShadow> const& getShadow() const;
        std::optional<SurfaceEffect> const& getEffect() const;
        std::shared_ptr<RenderTreeNode> const& getChild() const;

    private:
        std::optional<AnimationOptions> animationOptions_;
        ShapeFunction shape_;
        Animated<std::optional<Brush>> fill_;
        std::optional<SurfaceShadow> shadow_;
        std::optional<SurfaceEffect> effect_;
        std::shared_ptr<RenderTreeNode> childNode_;
    };
} // namespace avg
