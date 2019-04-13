#pragma once

#include "onpointerup.h"
#include "onpointerdown.h"

#include "reactive/signal/map.h"

#include "reactive/pointerbuttonevent.h"
#include "reactive/signal.h"
#include "reactive/clickevent.h"

#include <functional>
#include <type_traits>

namespace reactive::widget
{
    template <typename T, typename U, std::enable_if_t<
        std::is_convertible<T, std::function<void(ClickEvent const&)>>::value
        , int> = 0>
    inline auto onClick(unsigned int button, Signal<T, U> cb)
            //Signal<std::function<void(ClickEvent const&)>> cb)
    {
        auto pos = std::make_shared<btl::option<ase::Vector2f>>(btl::none);

        auto h = [button, pos](ase::PointerButtonEvent const& e)
        {
            if (button == 0 || e.button == button)
            {
                *pos = btl::just(e.pos);
                return EventResult::possible;
            }
            else
                return EventResult::reject;
        };

        auto f = [button, pos](
                std::function<void(ClickEvent const&)> const& cb,
                ase::PointerButtonEvent const& e)
        {
            float const threshold = 1.0f;

            if (pos->valid())
            {
                ase::Vector2f diff = e.pos - **pos;
                float dist2 = diff.x() * diff.x() + diff.y() * diff.y();

                if (dist2 < threshold && (button == 0 || e.button == button))
                {
                    *pos = btl::none;
                    cb(ClickEvent(e.pointer, e.button, e.pos));

                    return EventResult::accept;
                }
            }

            return EventResult::possible;
        };

        return combineWidgetMaps(
                onPointerDown(std::move(h)),
                onPointerUp(signal::mapFunction(std::move(f), std::move(cb)))
                );
    }

    template <typename T, typename U, std::enable_if_t<
        std::is_convertible<T, std::function<void()>>::value
        , int> = 0>
    inline auto onClick(unsigned int button, Signal<T, U> cb)
    {
        auto f = [](std::function<void()> cb, ClickEvent const&)
        {
            cb();
        };

        return onClick(button, signal::mapFunction(std::move(f), std::move(cb)));
    }

    inline auto onClick(unsigned int button, std::function<void(ClickEvent const&)> f)
    {
        return onClick(button, signal::constant(std::move(f)));
    }

} // namespace reactive


