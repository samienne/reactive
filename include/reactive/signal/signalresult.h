#pragma once

#include <btl/invoke.h>
#include <btl/all.h>
#include <btl/not.h>

#include <cstddef>
#include <type_traits>
#include <tuple>

namespace reactive::signal
{
    template <typename... Ts>
    class SignalResult;

    template <typename T>
    struct IsSignalResult : std::false_type {};

    template <typename... Ts>
    struct IsSignalResult<SignalResult<Ts...>> : std::true_type {};

    template <typename... Ts, typename TTuple, size_t... S>
    auto doConvertTuple(TTuple&& t, std::index_sequence<S...>)
    {
        return std::tuple<Ts...>(
                static_cast<Ts>(std::get<S>(std::forward<TTuple>(t)))...
                );
    }

    template <typename... Ts, typename... Us>
    auto convertTuple(std::tuple<Us...> const& t)
    {
        return doConvertTuple<Ts...>(t, std::make_index_sequence<sizeof...(Ts)>());
    }

    template <typename... Ts, typename... Us>
    auto convertTuple(std::tuple<Us...>& t)
    {
        return doConvertTuple<Ts...>(t, std::make_index_sequence<sizeof...(Ts)>());
    }

    template <typename... Ts, typename... Us>
    auto convertTuple(std::tuple<Us...>&& t)
    {
        return doConvertTuple<Ts...>(std::move(t), std::make_index_sequence<sizeof...(Ts)>());
    }

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
            values_(convertTuple<Ts...>(std::move(other.values_)))
        {
        }

        template <typename... Us, typename = std::enable_if_t<
            btl::All<
                std::is_convertible<Us, Ts>...
            >::value>>
        SignalResult(SignalResult<Us...> const& other) :
            values_(convertTuple<Ts...>(other.values_))
        {
        }

        SignalResult(SignalResult const&) = default;
        SignalResult(SignalResult&&) noexcept = default;

        SignalResult& operator=(SignalResult const& rhs)
        {
            values_.~tuple();
            new (&values_) std::tuple<Ts...>(rhs.values_);

            return *this;
        }

        SignalResult& operator=(SignalResult&& rhs) noexcept
        {
            values_.~tuple();
            new (&values_) std::tuple<Ts...>(std::move(rhs.values_));

            return *this;
        }

        /*
        template <typename T, typename = std::enable_if_t<
            btl::All<
                btl::Not<IsSignalResult<std::decay_t<T>>>,
                std::is_constructible<std::tuple<Ts...>, std::tuple<T>>
            >::value
            >>
        SignalResult& operator=(T&& t)
        {
            values_.~tuple();
            new (&values_) std::tuple<Ts...>(std::forward<T>(t));
            return *this;
        }
        */

        auto operator==(SignalResult const& rhs) const
            -> decltype(std::declval<std::tuple<Ts...>>()
                    == std::declval<std::tuple<Ts...>>())
        {
            return values_ == rhs.values_;
        }

        auto operator!=(SignalResult const& rhs) const
            -> decltype(std::declval<std::tuple<Ts...>>()
                    != std::declval<std::tuple<Ts...>>())
        {
            return values_ != rhs.values_;
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
    SignalResult<std::decay_t<Ts>...> makeSignalResult(Ts&&... ts)
    {
        return SignalResult<std::decay_t<Ts>...>{ std::forward<Ts>(ts)... };
    }

    template <typename... Ts>
    SignalResult<Ts...> makeSignalResult(SignalResult<Ts...> result)
    {
        return result;
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

    template <typename... Ts>
    auto concatSignalResults(Ts&&... ts)
    {
        return makeSignalResultFromTuple(std::tuple_cat(std::forward<Ts>(ts)
                    .getTuple()...));
    }

    template <typename... Ts>
    struct ToSignalResult
    {
        using type = SignalResult<Ts...>;
    };

    template <typename... Ts>
    struct ToSignalResult<SignalResult<Ts...>>
    {
        using type = SignalResult<Ts...>;
    };

    template <typename... Ts>
    using ToSignalResultT = typename ToSignalResult<Ts...>::type;

    template <typename T>
    struct UnpackSignalResult
    {
    };

    template <typename... Ts>
    struct UnpackSignalResult<SignalResult<Ts...>>
    {
        using type = btl::ParamPack<Ts...>;
    };

    template <typename T>
    using UnpackSignalResultT = typename UnpackSignalResult<T>::type;
} // reactive::signal

