#pragma once


#include "widget/addwidgets.h"
#include "widget/bindsize.h"
#include "widget/widgetobject.h"
#include "widget//widgettransformer.h"

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
                        std::vector<AnySignal<Widget>> result;

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
            | widget::makeWidgetTransformer()
            .compose(widget::bindSize())
            .values(hints.clone(), std::move(widgets))
            .bind([](auto size, auto hints, auto widgets) mutable
                {
                    return detail::doDynamicBox<dir>(
                            std::move(size),
                            std::move(widgets),
                            std::move(hints)
                            );
                })
            | setSizeHint(std::move(resultHint))
            ;
    }
} // namespace reactive
