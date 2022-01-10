#pragma once

#include "instancemodifier.h"

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

        return makeInstanceModifier([id](Instance instance, auto cb)
                {
                    auto areas = instance.getInputAreas();
                    if (!areas.empty()
                            && areas.back().getObbs().size() == 1
                            && areas.back().getObbs().front() == instance.getObb())
                    {
                        areas.back() = std::move(areas.back()).onMove(std::move(cb));
                    }
                    else
                    {
                        areas.push_back(
                                makeInputArea(id, instance.getObb()).onMove(std::move(cb))
                                );
                    }

                    return std::move(instance)
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

