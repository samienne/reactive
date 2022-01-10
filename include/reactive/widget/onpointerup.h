#pragma once

#include "setinputareas.h"
#include "widgetmodifier.h"

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

        return makeWidgetModifier([id](Instance instance, auto cb)
            {
                auto areas = instance.getInputAreas();

                if (!areas.empty()
                        && areas.back().getObbs().size() == 1
                        && areas.back().getObbs().front() == instance.getObb())
                {
                    areas.back() = std::move(areas.back()).onUp(std::move(cb));
                }
                else
                {
                    areas.push_back(
                            makeInputArea(id, instance.getObb()).onUp(std::move(cb))
                            );
                }

                return std::move(instance)
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

