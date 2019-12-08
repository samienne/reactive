#pragma once

#include "bindinputareas.h"
#include "bindobb.h"
#include "setinputareas.h"
#include "widgettransformer.h"

#include "reactive/signal/signal.h"

#include <ase/hoverevent.h>

namespace reactive::widget
{
    inline auto onHover(
            AnySignal<std::function<void(reactive::HoverEvent const&)>> cb,
            AnySignal<avg::Obb> area)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetTransformer()
            .compose(grabInputAreas())
            .values(std::move(area), std::move(cb))
            .bind([id](auto areas, auto area, auto cb)
            {
                auto newAreas = signal::map(
                    [id](std::vector<InputArea> areas, avg::Obb const& obb, auto cb)
                    {
                        if (!areas.empty()
                                && areas.back().getObbs().size() == 1
                                && areas.back().getObbs().front() == obb)
                        {
                            areas.back() = std::move(areas.back())
                                .onHover(std::move(cb));

                            return areas;
                        }

                        areas.push_back(
                                makeInputArea(id, obb).onHover(std::move(cb))
                                );

                        return areas;
                    },
                    std::move(areas),
                    std::move(area),
                    std::move(cb)
                    );

                return setInputAreas(std::move(newAreas));
            });
    }

    inline auto onHover(AnySignal<
            std::function<void(reactive::HoverEvent const&)>
            > cb)
    {
        auto id = btl::makeUniqueId();

        return makeWidgetTransformer()
            .compose(grabInputAreas(), bindObb())
            .values(std::move(cb))
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
                            areas.back() = std::move(areas.back()).onHover(std::move(cb));
                            return areas;
                        }

                        areas.push_back(
                                makeInputArea(id, obb).onHover(std::move(cb))
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

    inline auto onHover(
            std::function<void(reactive::HoverEvent const&)> cb
            )
    {
        return onHover(signal::constant(std::move(cb)));
    }
} // namespace reactive::widget

