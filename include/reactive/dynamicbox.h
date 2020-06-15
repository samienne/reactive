#pragma once

#include "widget/addwidgets.h"
#include "widget/bindsize.h"
#include "widget/binddrawcontext.h"
#include "widget/widgetobject.h"
#include "widget//widgettransformer.h"

#include "box.h"

namespace reactive
{
    namespace detail
    {
        template <Axis dir, typename T, typename U, typename V, typename W>
        auto doDynamicBox(
                Signal<T, DrawContext> drawContext,
                Signal<U, avg::Vector2f> size,
                Signal<V, std::vector<widget::WidgetObject>> widgets,
                SharedSignal<W, std::vector<SizeHint>> hints
                )
        {
            auto obbs = signal::map(&mapObbs<dir>, std::move(size), hints);

            auto resultWidgets = signal::map(
                    [](DrawContext const& drawContext,
                        std::vector<widget::WidgetObject> const& widgets,
                        std::vector<avg::Obb> const& obbs)
                    {
                        assert(widgets.size() == obbs.size());
                        std::vector<Widget> result;

                        auto i = obbs.begin();
                        for (auto& w : widgets)
                        {
                            widget::WidgetObject& widgetObject =
                                const_cast<widget::WidgetObject&>(w);

                            widgetObject.setObb(*i);
                            widgetObject.setDrawContext(drawContext);
                            result.push_back(w.getWidget().clone());
                            ++i;
                        }

                        return result;
                    },
                    std::move(drawContext),
                    std::move(widgets),
                    std::move(obbs)
                    );

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
            .compose(widget::bindDrawContext(), widget::bindSize())
            .values(hints.clone(), std::move(widgets))
            .bind([](auto drawContext, auto size, auto hints, auto widgets) mutable
                {
                    return detail::doDynamicBox<dir>(
                            std::move(drawContext),
                            std::move(size),
                            std::move(widgets),
                            std::move(hints)
                            );
                })
            | setSizeHint(std::move(resultHint))
            ;
    }
} // namespace reactive
