#pragma once

#include "widgetmodifier.h"
#include "buildermodifier.h"

namespace bqui::modifier
{
    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<bq::signal::AnySignal<SizeHint>, TFunc,
            bq::signal::AnySignal<SizeHint>, provider::ParamProviderTypeT<Ts>...
        >
    >>
    auto mapSizeHintSignal(TFunc&& func, Ts&&... ts) // -> AnyWidgetModifier
    {
        auto builderModifier = makeBuilderModifier([](auto builder,
                    auto func,
                    auto&&... ts)
            {
                auto sizeHint = func(builder.getSizeHint(),
                        std::forward<decltype(ts)>(ts)...);

                return std::move(builder).setSizeHint(std::move(sizeHint));
            },
            std::move(func),
            std::forward<Ts>(ts)...
            );

        return makeWidgetModifier(std::move(builderModifier));
    }

    template <typename TFunc, typename... Ts, typename... Us>
    auto mapSizeHint(TFunc&& func, bq::signal::Signal<Ts, Us>... sigs)
        // -> AnyWidgetModifier
    {
        return mapSizeHintSignal([](auto sizeHint, auto func, auto&&... sigs)
            {
                return merge(std::move(sizeHint), std::forward<decltype(sigs)>(sigs)...)
                    .map(std::move(func))
                    ;
            },
            std::forward<TFunc>(func),
            std::move(sigs)...
            );
    }
}
