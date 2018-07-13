#pragma once

#include "signal.h"
#include "pointermoveevent.h"
#include "widgetmap.h"

namespace reactive
{
    inline auto onPointerMove(Signal<
            std::function<void(ase::PointerMoveEvent const&)>
            > cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetMap<InputAreaTag, ObbTag>(
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
            std::move(cb)
            );
    }

    inline auto onPointerMove(
            std::function<void(ase::PointerMoveEvent const&)> cb
            )
    {
        return onPointerMove(signal::constant(std::move(cb)));
    }

} // namespace reactive

