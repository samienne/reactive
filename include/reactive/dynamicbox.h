#pragma once


#include "widget/addwidgets.h"
#include "widget/widgetmodifier.h"
#include "widget/widgetobject.h"

#include "box.h"

#include "signal/combine.h"

#include <avg/rendertree.h>

namespace reactive
{
    namespace detail
    {
        template <Axis dir, typename T, typename U, typename V>
        auto doDynamicBox(
                Signal<T, avg::Vector2f> size,
                Signal<U, std::vector<widget::WidgetObject>> widgets,
                SharedSignal<V, std::vector<SizeHint>> hints
                )
        {
            auto obbs = signal::map(&mapObbs<dir>, std::move(size), hints);

            auto resultWidgets = join(map(
                    [](std::vector<widget::WidgetObject> const& widgets,
                        std::vector<avg::Obb> const& obbs)
                    {
                        assert(widgets.size() == obbs.size());
                        std::vector<AnySignal<widget::Widget>> result;

                        auto i = obbs.begin();
                        for (auto& w : widgets)
                        {
                            widget::WidgetObject& widgetObject =
                                const_cast<widget::WidgetObject&>(w);

                            widgetObject.setObb(*i);
                            result.push_back(widgetObject.getWidget().clone());
                            ++i;
                        }

                        return combine(std::move(result));
                    },
                    std::move(widgets),
                    std::move(obbs)
                    ));

            return widget::addWidgets(std::move(resultWidgets));
        }
    } // namespace detail

    template <Axis dir, typename T>
    auto dynamicBox(Signal<T, std::vector<widget::WidgetObject>> widgets)
    {
        // Signal<std::vector<SizeHint>>
        auto hints = signal::share(signal::join(signal::map(
                [](std::vector<widget::WidgetObject> const& widgets)
                {
                    std::vector<AnySignal<SizeHint>> hints;

                    for (auto const& w : widgets)
                    {
                        hints.push_back(w.getSizeHint().clone());
                    }

                    // Signal<std::vector<SizeHint>>
                    return signal::combine(std::move(hints));
                },
                widgets.clone()
                )));

        //Signal<SizeHint>
        auto resultHint = signal::map(
                [](std::vector<SizeHint> hints) -> SizeHint
                {
                    return accumulateSizeHints<dir>(std::move(hints));
                },
                hints.clone()
                );

        return makeWidgetFactory()
            | widget::makeSharedWidgetSignalModifier(
                    [](auto widget, auto hints, auto widgets)
                    {
                        auto size = signal::map(&widget::Widget::getSize, widget);

                        return std::move(widget)
                            | detail::doDynamicBox<dir>(
                                    std::move(size),
                                    std::move(widgets),
                                    std::move(hints)
                                    )
                            ;
                    },
                    hints,
                    std::move(widgets)
                    )
            | setSizeHint(std::move(resultHint))
            ;
    }
} // namespace reactive
