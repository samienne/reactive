#include "bqui/modifier/surface.h"

#include "bqui/modifier/frame.h"
#include "bqui/modifier/instancemodifier.h"

#include "bqui/provider/provideanimation.h"

#include "bqui/shape/rectangle.h"

#include "bqui/widget/instance.h"

#include <bq/signal/constant.h>
#include <bq/signal/fromoptional.h>
#include <bq/signal/merge.h>

#include <avg/animated.h>
#include <avg/animationoptions.h>
#include <avg/builtineffects.h>
#include <avg/rendertree.h>
#include <avg/rendertree/surfacenode.h>
#include <avg/shapefunction.h>

#include <memory>
#include <utility>

namespace bqui::modifier
{

namespace
{
    struct ShadowValue
    {
        avg::Vector2f offset;
        float spread;
        avg::Brush brush;
    };

    bq::signal::AnySignal<std::optional<ShadowValue>> shadowValues(
            std::optional<SurfaceShadow> shadow)
    {
        if (!shadow)
            return bq::signal::constant(std::optional<ShadowValue>());

        return bq::signal::merge(
                std::move(shadow->offset),
                std::move(shadow->spread),
                std::move(shadow->brush)
                ).map([](avg::Vector2f offset, float spread, avg::Brush brush)
                {
                    return std::make_optional(ShadowValue{
                            offset, spread, std::move(brush) });
                });
    }

    std::optional<avg::SurfaceShadow> toNodeShadow(
            std::optional<ShadowValue> const& shadow)
    {
        if (!shadow)
            return std::nullopt;

        return avg::SurfaceShadow{
            avg::Animated<avg::Vector2f>(shadow->offset),
            avg::Animated<float>(shadow->spread),
            avg::Animated<avg::Brush>(shadow->brush)
        };
    }

    std::optional<avg::Animated<avg::Effect>> toNodeEffect(
            std::optional<avg::Effect> const& effect)
    {
        if (!effect)
            return std::nullopt;

        return avg::Animated<avg::Effect>(*effect);
    }

    widget::Instance makeSurfaceInstance(widget::Instance instance,
            std::optional<avg::AnimationOptions> const& animation,
            avg::ShapeFunction const& shape,
            std::optional<avg::Brush> const& fill,
            std::optional<ShadowValue> const& shadow,
            std::optional<avg::Effect> const& effect)
    {
        auto node = std::make_shared<avg::SurfaceNode>(
                instance.getObb(),
                animation,
                shape,
                avg::Animated<std::optional<avg::Brush>>(fill),
                toNodeShadow(shadow),
                toNodeEffect(effect),
                instance.getRenderTree().getRoot()
                );

        return std::move(instance)
            .setRenderTree(avg::RenderTree(std::move(node)))
            ;
    }
} // anonymous namespace

AnyWidgetModifier surface(
        shape::AnyShape shape,
        std::optional<bq::signal::AnySignal<avg::Brush>> fill,
        std::optional<SurfaceShadow> shadow,
        std::optional<bq::signal::AnySignal<avg::Effect>> effect
        )
{
    return makeWidgetModifier([](auto widget, auto animation, auto shape,
                auto fill, auto shadow, auto effect)
        {
            return std::move(widget)
                | makeWidgetModifier(makeInstanceModifier(
                            &makeSurfaceInstance,
                            std::move(animation),
                            std::move(shape),
                            std::move(fill),
                            std::move(shadow),
                            std::move(effect)
                            ))
                ;
        },
        provider::provideAnimation(),
        std::move(shape).getShapeFunction(),
        bq::signal::fromOptional(std::move(fill)),
        shadowValues(std::move(shadow)),
        bq::signal::fromOptional(std::move(effect))
        );
}

AnyWidgetModifier backdrop(shape::AnyShape shape,
        bq::signal::AnySignal<avg::Effect> effect)
{
    return surface(std::move(shape), std::nullopt, std::nullopt,
            std::move(effect));
}

AnyWidgetModifier frostedGlass(
        shape::AnyShape shape,
        bq::signal::AnySignal<avg::FrostedGlassOptions> options
        )
{
    return backdrop(std::move(shape), std::move(options).map(
                [](avg::FrostedGlassOptions const& value)
                {
                    return avg::frostedGlass(value);
                }));
}

AnyWidgetModifier frostedGlass(avg::FrostedGlassOptions options)
{
    return frostedGlass(
            shape::rectangle(bq::signal::constant(frameCornerRadius)),
            bq::signal::constant(std::move(options))
            );
}

AnyWidgetModifier tint(
        shape::AnyShape shape,
        bq::signal::AnySignal<avg::Color> colour,
        bq::signal::AnySignal<float> strength
        )
{
    return backdrop(std::move(shape), bq::signal::merge(
                std::move(colour),
                std::move(strength)
                ).map([](avg::Color const& value, float amount)
                {
                    return avg::tint(value, amount);
                }));
}

AnyWidgetModifier tint(
        bq::signal::AnySignal<avg::Color> colour,
        bq::signal::AnySignal<float> strength
        )
{
    return tint(shape::rectangle(), std::move(colour), std::move(strength));
}

} // namespace bqui::modifier
