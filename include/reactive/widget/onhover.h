#pragma once

#include "widgetmodifier.h"

#include "reactive/signal/signal.h"

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
} // namespace reactive::widget

