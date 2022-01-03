#pragma once

#include "bindinputareas.h"
#include "bindobb.h"
#include "setinputareas.h"
#include "widgetmodifier.h"

#include "reactive/eventresult.h"
#include "reactive/pointerbuttonevent.h"

#include "reactive/signal/signal.h"

#include <functional>

namespace reactive::widget
{
    template <typename T, typename = std::enable_if_t<
        signal::IsSignalType<
            std::decay_t<T>,
            std::function<EventResult(ase::PointerButtonEvent const&)>>::value
        >>
    inline auto onPointerDown(T&& cb)
    {
        btl::UniqueId id = btl::makeUniqueId();

        return makeWidgetModifier([id](Widget widget, auto cb)
            {
                auto areas = widget.getInputAreas();

                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == widget.getObb())
                {
                    areas.back() = std::move(areas.back()).onDown(std::move(cb));
                }
                else
                {
                    areas.push_back(
                            makeInputArea(id, widget.getObb())
                                .onDown(std::move(cb))
                            );
                }

                return std::move(widget)
                    .setInputAreas(std::move(areas))
                    ;
            },
            std::forward<T>(cb)
            );
    }

    inline auto onPointerDown(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb
            )
    {
        return onPointerDown(signal::constant(std::move(cb)));
    }
} // namespace reactive::widget

