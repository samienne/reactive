#pragma once

#include "onpointerup.h"
#include "onpointerdown.h"

#include "reactive/signal/map.h"

#include "reactive/pointerbuttonevent.h"
#include "reactive/clickevent.h"

#include "reactive/signal/signal.h"

#include <functional>
#include <type_traits>

namespace reactive::widget
{
    template <typename T, typename U, std::enable_if_t<
        //std::is_convertible<T, std::function<void(ClickEvent const&)>>::value
        std::is_invocable_v<T, ClickEvent>
        , int> = 0>
    inline auto onClick(unsigned int button, Signal<U, T> cb)
            //Signal<std::function<void(ClickEvent const&)>> cb)
    {
        auto f = [button](
                std::function<void(ClickEvent const&)> const& cb,
                ase::Vector2f size,
                ase::PointerButtonEvent const& e)
        {
            if (avg::Obb(size).contains(e.pos)
                    && (button == 0 || e.button == button))
            {
                cb(ClickEvent(e.pointer, e.button, e.pos));
                return EventResult::accept;
            }

            return EventResult::possible;
        };

        return makeSharedInstanceSignalModifier([](auto widget, auto f, auto cb)
                {
                    auto size = signal::map([](Instance const& w)
                            {
                                return w.getSize();
                            },
                            widget);

                    auto map = onPointerUp(
                            signal::mapFunction(std::move(f), cb, std::move(size)
                            ));

                    return std::move(widget)
                        | std::move(map)
                        ;
                },
                std::move(f),
                signal::share(std::move(cb))
                );
    }

    template <typename T, typename U, std::enable_if_t<
        std::is_invocable_v<T>
        //std::is_convertible<T, std::function<void()>>::value
        , int> = 0>
    inline auto onClick(unsigned int button, Signal<U, T> cb)
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


