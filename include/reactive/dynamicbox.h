#pragma once

#include "widget/providebuildparams.h"
#include "widget/addwidgets.h"
#include "widget/widgetobject.h"
#include "widget/instancemodifier.h"
#include "widget/builder.h"
#include "widget/widget.h"
#include "widget/widgetobject.h"
#include "widget/setsizehint.h"

#include "box.h"

#include "signal/combine.h"
#include "signal/foldp.h"

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
                        std::vector<AnySignal<widget::Instance>> result;

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
    auto dynamicBox(Signal<T, std::vector<std::pair<size_t, widget::AnyWidget>>> widgets)
    {
        return widget::makeWidget([](
                    reactive::widget::BuildParams const& params,
                    auto widgets
                    )
                {
                    auto widgetObjectsWithId = foldp([params](
                                std::vector<std::pair<size_t, widget::WidgetObject>> initial,
                                std::vector<std::pair<size_t, widget::AnyWidget>> widgets
                                )
                            -> std::vector<std::pair<size_t, widget::WidgetObject>>
                            {
                                std::vector<std::pair<size_t, widget::WidgetObject>> result;

                                for (auto&& widget : widgets)
                                {
                                    bool found = false;
                                    for (auto&& builder : initial)
                                    {
                                        if (builder.first == widget.first)
                                        {
                                            found = true;
                                            result.push_back(std::move(builder));
                                            break;
                                        }

                                    }

                                    if (!found)
                                    {
                                        result.emplace_back(
                                                widget.first,
                                                widget::WidgetObject(
                                                    std::move(widget.second),
                                                    params
                                                    )
                                                );
                                    }
                                }

                                return result;
                            },
                            std::vector<std::pair<size_t, widget::WidgetObject>>(),
                            std::move(widgets)
                            );

                    auto widgetObjects = share(map(
                            [](std::vector<std::pair<size_t,
                                widget::WidgetObject>> const& widgetObjects)
                            {
                                std::vector<widget::WidgetObject> result;
                                for (auto const& widgetObject : widgetObjects)
                                    result.push_back(widgetObject.second);

                                return result;
                            },
                            std::move(widgetObjectsWithId)
                            ));

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
                            widgetObjects
                            )));

                    //Signal<SizeHint>
                    auto resultHint = signal::map(
                            [](std::vector<SizeHint> hints) -> SizeHint
                            {
                                return accumulateSizeHints<dir>(std::move(hints));
                            },
                            hints
                            );

                    return widget::makeWidgetWithSize(
                            [](auto size, auto hints, auto widgetObjects)
                            {
                                //auto size = map(&widget::Instance::getSize, instance);

                                return widget::makeWidget()
                                    | detail::doDynamicBox<dir>(
                                            std::move(size),
                                            std::move(widgetObjects),
                                            std::move(hints)
                                            )
                                    ;
                            },
                            std::move(hints),
                            std::move(widgetObjects)
                            )
                        | widget::setSizeHint(std::move(resultHint))
                        ;
                },
                widget::provideBuildParams(),
                share(std::move(widgets))
                )
            ;
    }
} // namespace reactive
