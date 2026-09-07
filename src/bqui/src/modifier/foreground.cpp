#include "bqui/modifier/foreground.h"

#include "bqui/modifier/addwidgets.h"

#include "bqui/widget/boxvariables.h"
#include "bqui/widget/builder.h"
#include "bqui/widget/layoutspec.h"

#include <bq/signal/signal.h>

#include <avg/brush.h>

#include <optional>
#include <utility>

namespace bqui::modifier
{
    AnyWidgetModifier foreground(widget::AnyWidget fgWidget)
    {
        return makeWidgetModifier([](auto widget, auto fgWidget,
                    BuildParams const& params) -> widget::AnyWidget
        {
            auto builder = std::move(widget)(params);
            auto sizeHint = builder.getSizeHint();
            std::optional<widget::PureLayout> childPure =
                builder.getPureLayout();
            widget::BoxVariables childBox = builder.getBoxVariables();

            auto composed = widget::makeBuilder(
                [builder = std::move(builder), fgWidget = std::move(fgWidget)](
                        BuildParams const& params,
                        bq::signal::AnySignal<avg::Vector2f> size,
                        bq::signal::AnySignal<widget::LayoutSolution> solution)
                        -> widget::AnyElement
                {
                    auto s = std::move(size).share();
                    // The wrapped child is built through the solution-carrying
                    // interface, so a pure container under a foreground reads the
                    // one region solution to place its own children; the overlay
                    // is a leaf and takes only the size.
                    auto bgElement = builder.clone()(s.clone(),
                            std::move(solution));
                    auto fgElement = fgWidget.clone()(params)(s.clone());

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
                bq::signal::constant(avg::Vector2f(0.5f, 0.5f))
                );

            // The overlay is layout-transparent: the wrapped child's band and box
            // forward unchanged for the enclosing region to solve it in place.
            if (childPure)
            {
                composed.setPureLayout(std::move(childPure));
                composed.setBoxVariables(std::move(childBox));
            }

            return makeWidgetFromBuilder(std::move(composed));
        },
        std::move(fgWidget),
        provider::provideBuildParams()
        );
    }
}
