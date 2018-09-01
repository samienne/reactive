#pragma once

#include "onpointerup.h"
//#include "addpointerarea.h"

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
        auto f = [button](
                std::function<void(ClickEvent const&)> const& cb,
                ase::PointerButtonEvent const& e)
        {
            if (button == 0 || e.button == button)
            {
                cb(ClickEvent(e.pointer, e.button, e.pos));

                return EventResult::finish;
            }

            return EventResult::possible;
        };

        return onPointerUp(signal::mapFunction(std::move(f), std::move(cb)));
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
        auto g = [button, f = std::move(f)](ase::PointerButtonEvent const& e)
        {
            if (button == 0 || e.button == button)
                f(ClickEvent(e.pointer, e.button, e.pos));

            return EventResult::finish;
        };

        return onPointerUp(g);
    }

    /*
    template <typename... TTags, typename TFunc, typename... TSigs>
    auto onClick2(unsigned int button, TFunc&& f, TSigs... sigs)
    {
        return addPointerArea()
            .onDown([button](PointerButtonEvent const& e)
                    {
                        return EventResult::reject;
                    })
            .template onUp<TTags...>([button, f=std::forward<TFunc>(f)]
                    (PointerButtonEvent const& e)
                    {
                        return EventResult::reject;
                    })
        ;
    }
    */
} // namespace reactive


