#pragma once

#include "bindinputareas.h"
#include "bindobb.h"
#include "setinputareas.h"
#include "widgettransformer.h"

#include "reactive/signal.h"
#include "reactive/pointermoveevent.h"
#include "reactive/eventresult.h"

namespace reactive::widget
{
    template <typename T, typename = std::enable_if_t<
        IsSignalType<
            std::decay_t<T>,
            std::function<EventResult(ase::PointerMoveEvent const&)>>::value
        >>
    inline auto onPointerMove(/*Signal<
            std::function<void(ase::PointerMoveEvent const&)>
            > */ T&& cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetTransformer()
            .provide(grabInputAreas(), bindObb())
            .values(std::forward<T>(cb))
            .bind([id](auto areas, auto obb, auto cb)
            {
                auto newAreas = signal::map(
                    [id](std::vector<InputArea> areas, avg::Obb const& obb, auto cb)
                    -> std::vector<InputArea>
                    {
                        if (!areas.empty()
                                && areas.back().getObbs().size() == 1
                                && areas.back().getObbs().front() == obb)
                        {
                            areas.back() = std::move(areas.back()).onMove(std::move(cb));
                            return areas;
                        }

                        areas.push_back(
                                makeInputArea(id, obb).onMove(std::move(cb))
                                );

                        return areas;
                    },
                    std::move(areas),
                    std::move(obb),
                    std::move(cb)
                    );

                return setInputAreas(std::move(newAreas));
            });
    }

    inline auto onPointerMove(
            std::function<EventResult(ase::PointerMoveEvent const&)> cb
            )
    {
        return onPointerMove(signal::constant(std::move(cb)));
    }

} // namespace reactive::widget

