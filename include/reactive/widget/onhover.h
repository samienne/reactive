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

        return makeWidgetModifier([id](Widget widget, avg::Obb const& area, auto cb)
                {
                    auto areas = widget.getInputAreas();
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

                    return std::move(widget)
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

        return makeWidgetModifier([id](Widget widget, auto cb)
            {
                auto areas = widget.getInputAreas();
                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == widget.getObb())
                {
                    areas.back() = std::move(areas.back()).onHover(std::move(cb));
                }
                else
                {
                    areas.push_back(
                            makeInputArea(id, widget.getObb()).onHover(std::move(cb))
                            );
                }

                return std::move(widget)
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
        return makeWidgetSignalModifier([](auto widget, auto handle)
            {
                return std::move(widget)
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
        return makeWidgetSignalModifier([](auto widget, auto obb, auto handle)
            {
                return std::move(widget)
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

