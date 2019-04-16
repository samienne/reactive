#pragma once

#include "widget/widgetobject.h"
#include "box.h"

namespace reactive
{
    template <Axis dir>
    auto dynamicBox(Signal<std::vector<widget::WidgetObject>> widgets)
    {
        // Signal<std::vector<SizeHint>>
        Signal<std::vector<SizeHint>> hints = signal::share(signal::join(signal::map(
                [](std::vector<widget::WidgetObject> const& widgets)
                {
                    std::vector<Signal<SizeHint>> hints;

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
                    return combineSizeHints<dir>(std::move(hints));
                },
                hints.clone()
                );

        auto sizeInput = signal::input(avg::Vector2f(100.0f, 100.0f));

        auto obbs = signal::map(&mapObbs<dir>, std::move(sizeInput.signal),
                hints.clone());

        auto resultWidgets = signal::map(
                [](std::vector<widget::WidgetObject> const& widgets,
                    std::vector<avg::Obb> const& obbs)
                {
                    assert(widgets.size() == obbs.size());
                    std::vector<Widget> result;

                    auto i = obbs.begin();
                    for (auto& w : widgets)
                    {
                        const_cast<widget::WidgetObject&>(w).setObb(*i);
                        result.push_back(w.getWidget().clone());
                        ++i;
                    }

                    return result;
                },
                std::move(widgets),
                std::move(obbs)
                );

        return makeWidgetFactory()
            | trackSize(std::move(sizeInput.handle))
            | widget::addWidgets(std::move(resultWidgets))
            | setSizeHint(std::move(resultHint))
            ;
    }
} // namespace reactive
