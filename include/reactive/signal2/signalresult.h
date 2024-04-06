#pragma once

#include <btl/all.h>

#include <cstddef>
#include <type_traits>
#include <tuple>

namespace reactive::signal2
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

        explicit SignalResult(std::tuple<Ts...> t) : values_(std::move(t))
        {
        }

        template <typename... Us, typename = std::enable_if_t<
            btl::All<
                std::is_convertible<Us, Ts>...
            >::value>>
        SignalResult(SignalResult<Us...>&& other) :
            values_(std::move(other.values_))
        {
        }

        template <typename... Us, typename = std::enable_if_t<
            btl::All<
                std::is_convertible<Us, Ts>...
            >::value>>
        SignalResult(SignalResult<Us...> const& other) :
            values_(other.values_)
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

        std::tuple<Ts...>& getTuple() &
        {
            return values_;
        }

        std::tuple<Ts...> const& getTuple() const&
        {
            return values_;
        }

        std::tuple<Ts...>&& getTuple() &&
        {
            return std::move(values_);
        }

        SignalResult clone() const
        {
            return std::apply([](auto&&... values)
                    {
                        return SignalResult(std::forward<decltype(values)>(values)...);
                    }, values_);
        }

        template <typename... Us>
        friend class SignalResult;

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
    SignalResult<Ts...> makeSignalResultFromTuple(std::tuple<Ts...> t)
    {
        return SignalResult<Ts...>(std::move(t));
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

    template <typename T>
    struct IsSignalResult : std::false_type {};

    template <typename... Ts>
    struct IsSignalResult<SignalResult<Ts...>> : std::true_type {};
} // reactive::signal2

