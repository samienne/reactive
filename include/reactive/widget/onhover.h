#pragma once

#include "widgetmodifier.h"

#include "reactive/signal/signal.h"
#include <reactive/signal/inputhandle.h>

#include <ase/hoverevent.h>

namespace reactive::widget
{
    inline auto onHover(
            AnySignal<std::function<void(reactive::HoverEvent const&)>> cb,
            AnySignal<avg::Obb> area)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetModifier([id](Instance instance, avg::Obb const& area, auto cb)
                {
                    auto areas = instance.getInputAreas();
                    if (!areas.empty()
                            && areas.back().getObbs().size() == 1
                            && areas.back().getObbs().front() == area)
                    {
                        areas.back() = std::move(areas.back())
                            .onHover(std::move(cb));
                    }
                    else
                    {
                        areas.push_back(
                                makeInputArea(id, area).onHover(std::move(cb))
                                );
                    }

                    return std::move(instance)
                        .setInputAreas(std::move(areas))
                        ;
                },
                std::move(area),
                std::move(cb)
                );
    }

    inline auto onHover(AnySignal<
            std::function<void(reactive::HoverEvent const&)>
            > cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetModifier([id](Instance instance, auto cb)
            {
                auto areas = instance.getInputAreas();
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == instance.getObb())
                {
                    areas.back() = std::move(areas.back()).onHover(std::move(cb));
                }
                else
                {
                    areas.push_back(
                            makeInputArea(id, instance.getObb()).onHover(std::move(cb))
                            );
                }

                return std::move(instance)
                    .setInputAreas(std::move(areas))
                    ;
            },
            std::move(cb)
            );
    }

    inline auto onHover(
            std::function<void(reactive::HoverEvent const&)> cb
            )
    {
        return onHover(signal::constant(std::move(cb)));
    }

    inline auto onHover(signal::InputHandle<bool> handle)
    {
        return makeWidgetSignalModifier([](auto instance, auto handle)
            {
                return std::move(instance)
                    | onHover([handle=std::move(handle)](HoverEvent const& e) mutable
                        {
                            handle.set(e.hover);
                        })
                    ;
            },
            std::move(handle)
            );
    }

    template <typename T>
    auto onHover(Signal<T, avg::Obb> obb, signal::InputHandle<bool> handle)
    {
        return makeWidgetSignalModifier([](auto instance, auto obb, auto handle)
            {
                return std::move(instance)
                    | onHover(signal::constant([handle=std::move(handle)]
                        (HoverEvent const& e) mutable
                        {
                            handle.set(e.hover);
                        }), std::move(obb)
                        )
                    ;
            },
            std::move(obb),
            std::move(handle)
            );
    }
} // namespace reactive::widget

