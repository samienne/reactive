#pragma once

#include "shapenode.h"

#include "avg/animated.h"
#include "avg/brush.h"
#include "avg/pen.h"
#include "avg/drawing.h"
#include "avg/drawcontext.h"
#include "avg/avgvisibility.h"

#include <optional>
#include <memory>

namespace avg
{
    class AVG_EXPORT RectNode : public ShapeNode<Animated<float>,
        Animated<std::optional<Brush>>, Animated<std::optional<Pen>>>
    {
    public:
        RectNode(
                Animated<Obb> obb,
                std::optional<AnimationOptions> animationOptions,
                Animated<float> radius,
                Animated<std::optional<Brush>> brush,
                Animated<std::optional<Pen>> pen
                );

        std::shared_ptr<RenderTreeNode> clone() const override
        {
            return std::make_shared<RectNode>(
                    getObb(),
                    getAnimationOptions(),
                    std::get<0>(getData()),
                    std::get<1>(getData()),
                    std::get<2>(getData())
                    );
        }

    private:
        static Drawing drawRect(
                DrawContext const& drawContext,
                Vector2f size,
                float radius,
                std::optional<Brush> const& brush,
                std::optional<Pen> const& pen
                );
    };
} // namespace avg
