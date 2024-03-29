#include "rendertree.h"

namespace avg
{
    class AVG_EXPORT DrawNode : public RenderTreeNode
    {
    public:
        DrawNode(DrawNode const& rhs) = default;

        DrawNode(
                Animated<Obb> obb,
                ShapeFunction shapeFunction,
                Animated<std::optional<Brush>> brush,
                Animated<std::optional<Pen>> pen,
                std::optional<AnimationOptions> animationOptions
                );

        std::pair<Drawing, bool> draw(DrawContext const& context,
                Obb const& obb,
                std::chrono::milliseconds time) const final;

        UpdateResult update(
                RenderTree const& /*oldTree*/,
                RenderTree const& /*newTree*/,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        ShapeFunction shapeFunction_;
        Animated<std::optional<Brush>> brush_;
        Animated<std::optional<Pen>> pen_;
        std::optional<AnimationOptions> animationOptions_;
    };
} // namespace avg
