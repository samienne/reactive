#pragma once

#include "reactive/pointerbuttonevent.h"
#include "reactive/signal.h"
#include "reactive/widgetmap.h"

#include <functional>
#include <type_traits>

namespace reactive::widget
{
    template <typename T, typename U, typename = std::enable_if_t<
        std::is_convertible<
            T,
            std::function<void(ase::PointerButtonEvent const&)>
        >::value
        >>
    inline auto onPointerUp(Signal<T, U> cb)
            //std::function<void(ase::PointerButtonEvent const&)>> cb)
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
                    areas.back() = std::move(areas.back()).onUp(std::move(cb));
                    return areas;
                }

                areas.push_back(
                        makeInputArea(id, obb).onUp(std::move(cb))
                        );

                return areas;
            },
            std::move(cb)
            );
    }

    inline auto onPointerUp(
            std::function<void(ase::PointerButtonEvent const&)> cb)
    {
        return onPointerUp(signal::constant(std::move(cb)));
    }

    inline auto onPointerUp(std::function<void()> cb)
    {
        std::function<void(ase::PointerButtonEvent const& e)> f =
            std::bind(std::move(cb));
        return onPointerUp(f);
    }
} // namespace reactive::widget
