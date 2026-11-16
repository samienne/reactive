#include "bqui/modifier/shadowboundary.h"

#include "bqui/modifier/instancemodifier.h"

#include "bqui/provider/provideanimation.h"

#include "bqui/widget/instance.h"

#include <bq/signal/constant.h>

#include <avg/animated.h>
#include <avg/animationoptions.h>
#include <avg/rendertree.h>
#include <avg/rendertree/shadowboundarynode.h>

#include <memory>
#include <optional>
#include <utility>

namespace bqui::modifier
{

namespace
{
    template <avg::ShadowBoundaryNode::Content content>
    widget::Instance makeBoundaryInstance(widget::Instance instance,
            std::optional<avg::AnimationOptions> const& animation,
            avg::Vector2f const& offset,
            float radius,
            avg::Color const& colour)
    {
        auto node = std::make_shared<avg::ShadowBoundaryNode>(
                animation,
                avg::ShadowLight{
                    avg::Animated<avg::Vector2f>(offset),
                    avg::Animated<float>(radius),
                    avg::Animated<avg::Color>(colour)
                },
                content,
                instance.getRenderTree().getRoot()
                );

        return std::move(instance)
            .setRenderTree(avg::RenderTree(std::move(node)))
            ;
    }

    template <avg::ShadowBoundaryNode::Content content>
    AnyWidgetModifier boundaryModifier(ShadowLight light)
    {
        return makeWidgetModifier([](auto widget, auto animation,
                    auto offset, auto radius, auto colour)
            {
                return std::move(widget)
                    | makeWidgetModifier(makeInstanceModifier(
                                &makeBoundaryInstance<content>,
                                std::move(animation),
                                std::move(offset),
                                std::move(radius),
                                std::move(colour)
                                ))
                    ;
            },
            provider::provideAnimation(),
            std::move(light.offset),
            std::move(light.radius),
            std::move(light.colour)
            );
    }

    ShadowLight defaultLight()
    {
        return ShadowLight{
            bq::signal::constant(avg::Vector2f(0.0f, -4.0f)),
            bq::signal::constant(8.0f),
            bq::signal::constant(avg::Color(0.0f, 0.0f, 0.0f, 0.35f))
        };
    }
} // anonymous namespace

AnyWidgetModifier shadowBoundary(ShadowLight light)
{
    return boundaryModifier<avg::ShadowBoundaryNode::Content::flat>(
            std::move(light));
}

AnyWidgetModifier shadowBoundary()
{
    return shadowBoundary(defaultLight());
}

AnyWidgetModifier shadowAndBackdropBoundary(ShadowLight light)
{
    return boundaryModifier<avg::ShadowBoundaryNode::Content::scoped>(
            std::move(light));
}

AnyWidgetModifier shadowAndBackdropBoundary()
{
    return shadowAndBackdropBoundary(defaultLight());
}

} // namespace bqui::modifier
