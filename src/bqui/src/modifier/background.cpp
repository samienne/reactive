#include "bqui/modifier/background.h"

#include "bqui/modifier/addwidgets.h"

#include "bqui/provider/providetheme.h"

#include "bqui/shape/rectangle.h"

#include "bqui/widget/boxvariables.h"
#include "bqui/widget/builder.h"
#include "bqui/widget/layoutspec.h"

#include <bq/signal/signal.h>

#include <avg/brush.h>

namespace bqui::modifier
{
    AnyWidgetModifier background(bq::signal::AnySignal<avg::Brush> brush)
    {
        return background(shape::rectangle().fill(std::move(brush)));
    }

    AnyWidgetModifier background()
    {
        return makeWidgetModifier([](auto widget, auto theme)
            {
                auto bg = std::move(theme).map([](Theme const& theme)
                    {
                        return avg::Brush(theme.getBackground());
                    });

                return std::move(widget)
                    | background(std::move(bg))
                    ;
            },
            provider::provideTheme()
            );
    }

    AnyWidgetModifier background(widget::AnyWidget bgWidget)
    {
        return makeWidgetModifier([](auto widget, auto bgWidget,
                    BuildParams const& params) -> widget::AnyWidget
        {
            auto builder = std::move(widget)(params);
            auto sizeHint = builder.getSizeHint();
            auto gravity = builder.getGravity();
            std::optional<widget::PureLayout> childPure =
                builder.getPureLayout();
            widget::BoxVariables childBox = builder.getBoxVariables();

            auto framed = widget::makeBuilder(
                [builder = std::move(builder), bgWidget = std::move(bgWidget)](
                        BuildParams const& params,
                        bq::signal::AnySignal<avg::Vector2f> size,
                        bq::signal::AnySignal<widget::LayoutSolution> solution)
                        -> widget::AnyElement
                {
                    auto s = std::move(size).share();
                    // The foreground child is built through the solution-carrying
                    // interface, so a framed pure container reads the one region
                    // solution to place its own children; the background shape is
                    // a leaf and takes only the size.
                    auto fgElement = builder.clone()(s.clone(),
                            std::move(solution));
                    auto bgElement = bgWidget.clone()(params)(s.clone());

                    auto newInstance = merge(
                            std::move(fgElement).getInstance(),
                            std::move(bgElement).getInstance()).map(
                            [](auto fgInstance, auto bgInstance)
                            {
                                return addWidgets(std::move(bgInstance),
                                        { std::move(fgInstance) });
                            });

                    return makeElement(std::move(newInstance), params);
                },
                std::move(sizeHint),
                params,
                std::move(gravity)
                );

            // The frame is layout-transparent: its margin insets the background
            // shape, not the foreground child, so the child's band forwards
            // unchanged for the enclosing region to solve the child in place.
            if (childPure)
            {
                framed.setPureLayout(std::move(childPure));
                framed.setBoxVariables(std::move(childBox));
            }

            return makeWidgetFromBuilder(std::move(framed));
        },
        std::move(bgWidget),
        provider::provideBuildParams()
        );
    }
}

