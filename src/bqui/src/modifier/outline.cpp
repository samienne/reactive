#include "bqui/modifier/outline.h"

#include "bqui/modifier/instancemodifier.h"

#include "bqui/provider/provideanimation.h"

#include "bqui/widget/instance.h"

#include <bq/signal/constant.h>

#include <avg/animationoptions.h>
#include <avg/rendertree.h>
#include <avg/rendertree/outlinenode.h>
#include <avg/shapefunction.h>

#include <memory>
#include <optional>
#include <utility>

namespace bqui::modifier
{

namespace
{
    template <avg::OutlineNode::Mode mode>
    widget::Instance makeOutlineInstance(widget::Instance instance,
            std::optional<avg::AnimationOptions> const& animation,
            std::optional<avg::ShapeFunction> const& outline)
    {
        auto node = std::make_shared<avg::OutlineNode>(
                instance.getObb(),
                animation,
                mode,
                outline,
                instance.getRenderTree().getRoot()
                );

        return std::move(instance)
            .setRenderTree(avg::RenderTree(std::move(node)))
            ;
    }

    template <avg::OutlineNode::Mode mode>
    AnyWidgetModifier outlineModifier(
            bq::signal::AnySignal<std::optional<avg::ShapeFunction>> outline)
    {
        return makeWidgetModifier([](auto widget, auto animation,
                    auto outline)
            {
                return std::move(widget)
                    | makeWidgetModifier(makeInstanceModifier(
                                &makeOutlineInstance<mode>,
                                std::move(animation),
                                std::move(outline)
                                ))
                    ;
            },
            provider::provideAnimation(),
            std::move(outline)
            );
    }
} // anonymous namespace

AnyWidgetModifier outline(
        bq::signal::AnySignal<std::optional<avg::ShapeFunction>> outline)
{
    return outlineModifier<avg::OutlineNode::Mode::replace>(
            std::move(outline));
}

AnyWidgetModifier addOutline(bq::signal::AnySignal<avg::ShapeFunction> outline)
{
    return outlineModifier<avg::OutlineNode::Mode::add>(
            std::move(outline).map([](avg::ShapeFunction func)
                {
                    return std::make_optional(std::move(func));
                }));
}

AnyWidgetModifier noOutline()
{
    return outline(bq::signal::constant(
                std::optional<avg::ShapeFunction>()));
}

} // namespace bqui::modifier
