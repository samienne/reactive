#pragma once

#include "reactive/signal.h"
#include "reactive/pointerbuttonevent.h"
#include "reactive/widgetmap.h"

#include <functional>

namespace reactive::widget
{
    inline auto onPointerDown(Signal<
            std::function<void(ase::PointerButtonEvent const&)>
            > cb)
    {
        btl::UniqueId id = btl::makeUniqueId();

        return makeWidgetMap<InputAreaTag, ObbTag>(
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
            std::move(cb)
            );
    }

    inline auto onPointerDown(
            std::function<void(ase::PointerButtonEvent const&)> cb
            )
    {
        return onPointerDown(signal::constant(std::move(cb)));
    }
} // namespace reactive::widget

