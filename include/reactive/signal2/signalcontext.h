#pragma once

#include "signal.h"

namespace reactive::signal2
{
    template <typename... Ts>
    class SignalContext
    {
    public:
        SignalContext(AnySignal<Ts...> sig) :
            sig_(std::move(sig)),
            data_(sig_.initialize()),
            result_(sig_.evaluate(data_))
        {
        };

        auto evaluate() const -> decltype(auto)
        {
            if constexpr(sizeof...(Ts) == 0)
                sig_.evaluate(data_);
            if constexpr (sizeof...(Ts) == 1)
                return result_.template get<0>();
            else
                return result_;
        }

        UpdateResult update(FrameInfo const& frame)
        {
            auto r = sig_.update(data_, frame);
            new(&result_) std::decay_t<SignalTypeT<AnySignal<Ts...>>>(sig_.evaluate(data_));
            return r;
        }

        template <typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Ts, Us>...)
            >>
        operator SignalContext<Us...>() const
        {
            return SignalContext<Us...>(sig_);
        }

    private:
        AnySignal<Ts...> sig_;
        typename AnySignal<Ts...>::DataType data_;
        std::decay_t<SignalTypeT<AnySignal<Ts...>>> result_;
    };

    template <typename T>
    struct ResultToContext
    {
    };

    template <typename... Ts>
    struct ResultToContext<SignalResult<Ts...>>
    {
        using type = SignalContext<Ts...>;
    };

    template <typename TSignal>
    using SignalToContext = typename ResultToContext<SignalTypeT<TSignal>>::type;

    template <typename TStorage, typename... Ts>
    SignalToContext<Signal<TStorage, Ts...>> makeSignalContext(
            Signal<TStorage, Ts...> sig)
    {
        return SignalToContext<Signal<TStorage, Ts...>>(std::move(sig));
    }
} // namespace reactive::signal2

