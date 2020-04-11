#pragma once

#include <btl/all.h>

#include <type_traits>
#include <tuple>

namespace reactive::signal
{
    template <typename... Ts>
    class SignalResult
    {
    public:
        template <typename... Us, typename = std::enable_if_t<
            btl::All<
                std::is_convertible<Us, Ts>...
                >::value
                >>
        explicit SignalResult(Us&&... us) : values_(std::forward<Us>(us)...)
        {
        }

        template <typename... Us>
        SignalResult(SignalResult<Us...> other) :
            values_(std::move(other.values_))
        {
        }

        template <size_t Index>
        auto get() & -> decltype(auto)
        {
            return std::get<Index>(values_);
        }

        template <size_t Index>
        auto get() const & -> decltype(auto)
        {
            return std::get<Index>(values_);
        }

        template <size_t Index>
        auto get() && -> decltype(auto)
        {
            return std::get<Index>(std::move(values_));
        }

        SignalResult clone() const
        {
            return std::apply([](auto&&... values)
                    {
                        return SignalResult(std::forward<decltype(values)>(values)...);
                    }, values_);
        }

    private:
        std::tuple<Ts...> values_;
    };

    template <size_t Index, typename... Ts>
    auto get(SignalResult<Ts...> const& result) -> decltype(auto)
    {
        return result.template get<Index>();
    }

    template <size_t Index, typename... Ts>
    auto get(SignalResult<Ts...>& result) -> decltype(auto)
    {
        return result.template get<Index>();
    }

    template <size_t Index, typename... Ts>
    auto get(SignalResult<Ts...>&& result) -> decltype(auto)
    {
        return std::move(result).template get<Index>();
    }

    template <typename... Ts>
    struct IsSignalResultType2 : std::false_type {};

    template <typename... Ts, typename... Us>
    struct IsSignalResultType2<SignalResult<Us...>, Ts...> :
        btl::All<
            std::is_convertible<std::decay_t<Us>, Ts>...
        >{};

    template <typename... Ts>
    struct IsSignalResultType : std::false_type {};

    template <typename... Ts, typename... Us>
    struct IsSignalResultType<SignalResult<Us...>, Ts...> :
        btl::All<
            std::integral_constant<bool, sizeof...(Ts) == sizeof...(Us)>,
            IsSignalResultType2<SignalResult<Us...>, Ts...>
        >{};
} // reactive::signal

