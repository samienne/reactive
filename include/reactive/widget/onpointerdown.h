#pragma once

#include "bindinputareas.h"
#include "bindobb.h"
#include "setinputareas.h"
#include "widgettransformer.h"

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

        return makeWidgetTransformer()
            .compose(grabInputAreas(), bindObb())
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
                            areas.back() = std::move(areas.back()).onDown(std::move(cb));
                            return areas;
                        }

                        areas.push_back(
                                makeInputArea(id, obb).onDown(std::move(cb))
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

    inline auto onPointerDown(
            std::function<EventResult(ase::PointerButtonEvent const&)> cb
            )
    {
        return onPointerDown(signal::constant(std::move(cb)));
    }
} // namespace reactive::widget

