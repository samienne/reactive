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

        return mapWidget([f=std::move(f), cb=signal::share(std::move(cb))]
            (auto widget) mutable
            {
                auto w2 = std::move(widget)
                    .setObb(signal::share(widget.getObb()))
                    ;

                auto map = onPointerUp(
                        signal::mapFunction(std::move(f), cb, w2.getSize()
                        ));

                return std::move(map)(std::move(w2));
                ;
            });
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


