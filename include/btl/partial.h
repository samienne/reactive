#pragma once

#include "apply.h"
#include "typetraits.h"

#include <utility>

namespace btl
{

    template <typename TFunc, typename... Ts>
    class Partial
    {
    public:
        Partial(Partial const&) = default;
        Partial(Partial&&) noexcept = default;
        Partial& operator=(Partial const&) = default;
        Partial& operator=(Partial&&) noexcept = default;

        /*template <typename TFunc2, typename... Us, typename = typename
            std::enable_if
            <
                std::is_assignable<TFunc, TFunc2>::value
            >::type>
        Partial(TFunc2&& func, Us&&... us) :
            func_(std::forward<TFunc2>(func)),
            ts_(std::forward<Us>(us)...)
        {
        }*/

        template <typename TFunc2, typename... Us>
        Partial(Partial<TFunc2, Us...> const& rhs) :
            func_(rhs.func_),
            ts_(rhs.ts_)
        {
        }

        Partial(TFunc&& func, Ts&&... ts) :
            func_(std::forward<TFunc>(func)),
            ts_(std::forward<Ts>(ts)...)
        {
        }

        template<typename... Us>
        auto operator()(Us&&... us) const
        -> typename std::decay<decltype(btl::apply(std::declval<TFunc const>(),
                    std::declval<std::tuple<
                    typename std::decay<Ts>::type...,
                    typename std::decay<Us>::type...
                    >>()))>::type
        {
            return btl::apply(func_, tuple_cat(ts_,
                        std::make_tuple(std::forward<Us>(us)...)));
        }

        template<typename... Us>
        auto operator()(Us&&... us)
        -> typename std::decay<decltype(btl::apply(std::declval<TFunc>(),
                    std::declval<std::tuple<
                    typename std::decay<Ts>::type...,
                    typename std::decay<Us>::type...
                    >>()))>::type
        {
            return btl::apply(func_, tuple_cat(ts_,
                        std::make_tuple(std::forward<Us>(us)...)));
        }

    private:
        template <typename TFunc2, typename... Us>
        friend class Partial;

        typename std::decay<TFunc>::type func_;
        std::tuple<typename std::decay<Ts>::type...> ts_;
    };

    template <typename TFunc, typename... Ts, typename = typename
        std::enable_if<
            !btl::CanApply<TFunc(Ts...)>::value
        >::type>
    auto applyPartial(TFunc&& func, Ts&&... ts)
        -> Partial<TFunc, Ts...>
    {
        return Partial< TFunc, Ts...>(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...);
    }

    template <typename TFunc, typename... Ts, typename = typename
        std::enable_if<
            btl::CanApply<TFunc(Ts...)>::value
        >::type>
    auto applyPartial(TFunc&& func, Ts&&... ts)
        -> typename std::decay
        <
            decltype(std::declval<TFunc>()(std::declval<Ts>()...))
        >::type
    {
        return std::forward<TFunc>(func)(std::forward<Ts>(ts)...);
    }

    template <typename TFunc, typename... Ts>
    auto applyPartialFunction(TFunc&& func, Ts&&... ts)
        -> Partial<typename std::decay<TFunc>::type, Ts...>
    {
        return Partial<TFunc, Ts...>(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...);
    }

} // btl

