#pragma once

#include "share.h"

#include "reactive/widget.h"

#include "reactive/signal.h"

namespace reactive::widget
{
    template <typename TFunc, typename TSignals, typename... TTags>
    class FunctionMapper
    {
    public:
        FunctionMapper(TFunc f, TSignals sigs) :
            func_(std::move(f)),
            sigs_(btl::cloneOnCopy(std::move(sigs)))
        {
        }

        template <typename... Ts>
        auto operator()(Wid<Ts...> w)
        {
            auto w2 = share<TTags...>(std::move(w));

            auto cb = [func=func_]()
            {
            };

            return std::make_tuple(std::move(w2), std::move(cb));
        }

    private:
        TFunc func_;
        btl::CloneOnCopy<TSignals> sigs_;
    };

    template <typename... TTags, typename TFunc, typename... Ts, typename... Us>
    auto callWithTags(TFunc&& f, Signal<Ts, Us>... sigs)
    {
        return FunctionMapper<
            std::decay_t<TFunc>,
            std::tuple<Signal<Ts, Us>...>,
            TTags...>(
                    std::forward<TFunc>(f),
                    std::make_tuple(std::move(sigs)...)
                  );
    }
} // namespace reactive::widget

