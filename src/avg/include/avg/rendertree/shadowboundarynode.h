#pragma once

#include "rendertreenode.h"

#include "avg/animated.h"
#include "avg/animationoptions.h"
#include "avg/color.h"
#include "avg/drawcontext.h"
#include "avg/drawing.h"
#include "avg/obb.h"
#include "avg/vector.h"
#include "avg/avgvisibility.h"

#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <utility>

namespace avg
{
    /**
     * @brief The light of a shadow boundary, in logical units: the shadow
     * is the casters moved by offset, blurred by a Gaussian reaching radius
     * and filled with colour, whose alpha is the shadow's opacity.
     */
    struct ShadowLight
    {
        Animated<Vector2f> offset;
        Animated<float> radius;
        Animated<Color> colour;
    };

    /**
     * @brief Paints the shadow its child's outlines cast under the child.
     *
     * The child's outlines are consumed: their union, moved and blurred by
     * the light, is filled once under the child's drawing and the drawing
     * leaves the node with no outlines, so an outer boundary sees none of
     * them. Overlapping casters therefore do not darken twice, and casters
     * under one boundary do not shadow each other. With Content::scoped the
     * child's drawing is put in a BackdropScope after the shadow, so the
     * child's backdrop effects see the shadow. The shadow is not clipped
     * or culled here; the renderer culls it like any element. Like
     * OutlineNode, the node's own obb only carries placement and moves with
     * the options the parent passes; the animation options apply to the
     * light.
     */
    class AVG_EXPORT ShadowBoundaryNode : public RenderTreeNode
    {
    public:
        enum class Content
        {
            flat,
            scoped
        };

        ShadowBoundaryNode(
                std::optional<AnimationOptions> animationOptions,
                ShadowLight light,
                Content content,
                std::shared_ptr<RenderTreeNode> childNode
                );

        ShadowBoundaryNode(ShadowBoundaryNode const&) = default;
        ShadowBoundaryNode& operator=(ShadowBoundaryNode const&) = default;

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

        ShadowLight const& getLight() const;
        Content getContent() const;
        std::shared_ptr<RenderTreeNode> const& getChild() const;

    private:
        ShadowBoundaryNode(
                Animated<Obb> obb,
                std::optional<AnimationOptions> animationOptions,
                ShadowLight light,
                Content content,
                std::shared_ptr<RenderTreeNode> childNode
                );

        std::optional<AnimationOptions> animationOptions_;
        ShadowLight light_;
        Content content_;
        std::shared_ptr<RenderTreeNode> childNode_;
    };
} // namespace avg
