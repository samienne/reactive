#pragma once

#include "widgetmodifier.h"

#include "reactive/signal/signal.h"

#include "reactive/pointermoveevent.h"
#include "reactive/eventresult.h"

namespace reactive::widget
{
    template <typename T, typename = std::enable_if_t<
        signal::IsSignalType<
            std::decay_t<T>,
            std::function<EventResult(ase::PointerMoveEvent const&)>>::value
        >>
    inline auto onPointerMove(/*Signal<
            std::function<void(ase::PointerMoveEvent const&)>
            > */ T&& cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetModifier([id](Widget widget, auto cb)
                {
                    auto areas = widget.getInputAreas();
                    if (!areas.empty()
                            && areas.back().getObbs().size() == 1
                            && areas.back().getObbs().front() == widget.getObb())
                    {
                        areas.back() = std::move(areas.back()).onMove(std::move(cb));
                    }
                    else
                    {
                        areas.push_back(
                                makeInputArea(id, widget.getObb()).onMove(std::move(cb))
                                );
                    }

                    return std::move(widget)
                        .setInputAreas(std::move(areas))
                        ;
                },
                std::forward<T>(cb)
                );
    }

    inline auto onPointerMove(
            std::function<EventResult(ase::PointerMoveEvent const&)> cb
            )
    {
        return onPointerMove(signal::constant(std::move(cb)));
    }

} // namespace reactive::widget

