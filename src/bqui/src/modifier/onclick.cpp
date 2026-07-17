#include "bqui/modifier/onclick.h"

#include "bqui/modifier/onpointerup.h"
#include "bqui/modifier/setwidgetintrospection.h"

#include "bqui/widget/introspection.h"

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

    auto clickModifier = makeWidgetModifierWithSize(
        [](auto widget, auto size, auto f, auto cb)
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

    // The click behaviour is applied through a size-aware modifier that lowers
    // to Element (dropping introspection), so contribute the Clickable
    // capability and the node's obb at the widget level, outside that lowering.
    return makeWidgetModifier(
        [](auto widget, auto clickModifier)
        {
            return std::move(widget)
                | std::move(clickModifier)
                | addCapability(widget::Capability::Clickable)
                | setIntrospectionObb()
                ;
        },
        std::move(clickModifier)
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

