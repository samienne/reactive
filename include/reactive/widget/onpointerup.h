#pragma once

#include "bindinputareas.h"
#include "bindobb.h"
#include "setinputareas.h"
#include "widgettransformer.h"

#include "reactive/signal/signal.h"

#include "reactive/pointerbuttonevent.h"
#include "reactive/eventresult.h"

#include <functional>
#include <type_traits>

namespace reactive::widget
{
    template <typename T, typename U, typename = std::enable_if_t<
        std::is_invocable_r_v<EventResult, T, ase::PointerButtonEvent>
        >>
    inline auto onPointerUp(Signal<U, T> cb)
            //std::function<void(ase::PointerButtonEvent const&)>> cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetModifier([id](auto widget, auto cb)
            {
                auto areas = widget.getInputAreas();

                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == widget.getObb())
                {
                    areas.back() = std::move(areas.back()).onUp(std::move(cb));
                }
                else
                {
                    areas.push_back(
                            makeInputArea(id, widget.getObb()).onUp(std::move(cb))
                            );
                }

                return std::move(widget)
                    .setInputAreas(std::move(areas))
                    ;
            },
            std::move(cb)
            );
    }

    inline auto onPointerUp(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb)
    {
        return onPointerUp(signal::constant(std::move(cb)));
    }

    inline auto onPointerUp(std::function<EventResult()> cb)
    {
        std::function<EventResult(ase::PointerButtonEvent const& e)> f =
            std::bind(std::move(cb));
        return onPointerUp(f);
    }
} // namespace reactive::widget

