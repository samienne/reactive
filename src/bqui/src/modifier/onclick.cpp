#include "bqui/modifier/onclick.h"

#include "bqui/modifier/onpointerup.h"

#include "bqui/clickevent.h"

#include <bq/signal/signal.h>

#include <functional>
#include <type_traits>

namespace bqui::modifier
{

AnyWidgetModifier onClick(unsigned int button,
        bq::signal::AnySignal<std::function<void(ClickEvent const&)>> cb)
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

    return makeWidgetModifierWithSize([](auto widget, auto size, auto f, auto cb)
        {
            return std::move(widget)
                | onPointerUp(
                        merge(std::move(cb), std::move(size))
                        .bindToFunction(std::move(f)))
                ;
        },
        std::move(f),
        std::move(cb).share()
        );
}

AnyWidgetModifier onClick(unsigned int button,
        bq::signal::AnySignal<std::function<void()>> cb)
{
    auto f = [](std::function<void()> cb, ClickEvent const&)
    {
        cb();
    };

    auto c = std::move(cb).bindToFunction(std::move(f));
    return onClick(button, c.template cast<std::function<void(ClickEvent const&)>>());
}

AnyWidgetModifier onClick(unsigned int button, std::function<void(ClickEvent const&)> f)
{
    return onClick(button, bq::signal::constant(std::move(f)));
}

}

