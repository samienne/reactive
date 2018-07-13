#pragma once

#include "signal.h"
#include "widgetmap.h"

#include <ase/hoverevent.h>

namespace reactive
{
    inline auto onHover(Signal<
            std::function<void(reactive::HoverEvent const&)>
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
                    areas.back() = std::move(areas.back()).onHover(std::move(cb));
                    return areas;
                }

                areas.push_back(
                        makeInputArea(id, obb).onHover(std::move(cb))
                        );

                return areas;
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
} // namespace reactive
