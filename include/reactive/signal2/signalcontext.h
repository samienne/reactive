#pragma once

#include "signal.h"

namespace reactive::signal2
{
    template <typename... Ts>
    class SignalContext
    {
    public:
        using InnerResultType = std::optional<SignalResult<Ts...>>;

        SignalContext(AnySignal<Ts...> sig) :
            sig_(std::move(sig)),
            data_(sig_.unwrap().initialize()),
            result_(sig_.unwrap().evaluate(data_))
        {
        };

        auto evaluate() const -> decltype(auto)
        {
            if constexpr(sizeof...(Ts) == 0)
                sig_.unwrap().evaluate(data_);
            if constexpr (sizeof...(Ts) == 1)
                return result_->template get<0>();
            else
                return SignalResult<Ts...>(*result_);
        }

        UpdateResult update(FrameInfo const& frame)
        {
            auto r = sig_.unwrap().update(data_, frame);
            if (r.didChange)
                result_ = sig_.unwrap().evaluate(data_);

            return r;
        }

        Connection observe(std::function<void()> callback)
        {
            return sig_.unwrap().observe(data_, std::move(callback));
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
        InnerResultType result_;
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
    SignalToContext<typename Signal<TStorage, Ts...>::StorageType> makeSignalContext(
            Signal<TStorage, Ts...> sig)
    {
        return SignalToContext<typename Signal<TStorage, Ts...>::StorageType>(
                std::move(sig)
                );
    }
} // namespace reactive::signal2

