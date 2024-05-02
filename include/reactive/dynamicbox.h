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

#include "signal2/combine.h"
#include "signal2/signal.h"

#include <avg/rendertree.h>

namespace reactive
{
    namespace detail
    {
        template <Axis dir, typename T, typename U, typename V>
        auto doDynamicBox(
                signal2::Signal<T, avg::Vector2f> size,
                signal2::Signal<U, std::vector<widget::WidgetObject>> widgets,
                signal2::Signal<V, std::vector<SizeHint>> hints
                )
        {
            auto obbs = merge(std::move(size), hints).map(&mapObbs<dir>);

            auto resultWidgets = merge(widgets, obbs)
                .map([](std::vector<widget::WidgetObject> const& widgets,
                        std::vector<avg::Obb> const& obbs)
                    {
                        assert(widgets.size() == obbs.size());
                        std::vector<signal2::AnySignal<widget::Instance>> result;

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
                    })
                .join();

            return widget::addWidgets(std::move(resultWidgets));
        }
    } // namespace detail

    template <Axis dir>
    widget::AnyWidget dynamicBox(
            signal2::AnySignal<std::vector<std::pair<size_t, widget::AnyWidget>>> widgets)
    {
        return widget::makeWidget([](
                    reactive::widget::BuildParams const& params,
                    auto widgets
                    )
                {
                    auto widgetObjectsWithId = std::move(widgets)
                    .withPrevious([params](
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
                            std::vector<std::pair<size_t, widget::WidgetObject>>()
                            );

                    auto widgetObjects = std::move(widgetObjectsWithId)
                        .map([](std::vector<std::pair<size_t,
                                widget::WidgetObject>> const& widgetObjects)
                            {
                                std::vector<widget::WidgetObject> result;
                                for (auto const& widgetObject : widgetObjects)
                                    result.push_back(widgetObject.second);

                                return result;
                            })
                        .share();

                    // Signal<std::vector<SizeHint>>
                    auto hints = widgetObjects.map(
                            [](std::vector<widget::WidgetObject> const& widgets)
                            {
                                std::vector<signal2::AnySignal<SizeHint>> hints;

                                for (auto const& w : widgets)
                                {
                                    hints.push_back(w.getSizeHint().clone());
                                }

                                // Signal<std::vector<SizeHint>>
                                return signal2::combine(std::move(hints));
                            }).join().share();

                    //Signal<SizeHint>
                    auto resultHint = hints.map(
                            [](std::vector<SizeHint> hints) -> SizeHint
                            {
                                return accumulateSizeHints<dir>(std::move(hints));
                            });

                    return widget::makeWidgetWithSize(
                            [](auto size, auto hints, auto widgetObjects)
                            {
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
                std::move(widgets).share()
                )
            ;
    }
} // namespace reactive
