#pragma once

#include "typetraits.h"
#include "forcenoexcept.h"

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

        template <typename TFunc2, typename... Us>
        Partial(Partial<TFunc2, Us...> const& rhs) :
            func_(rhs.func_),
            ts_(rhs.ts_)
        {
        }

        Partial(TFunc&& func, Ts&&... ts) :
            func_(forceNoexcept(std::forward<TFunc>(func))),
            ts_(forceNoexcept(std::make_tuple(std::forward<Ts>(ts)...)))
        {
        }

        template<typename... Us>
        auto operator()(Us&&... us) const
        -> std::decay_t<std::invoke_result_t<std::decay_t<TFunc> const&,
            std::decay_t<Ts> const..., Us...>>
        {
            return std::apply(*func_, tuple_cat(*ts_,
                        std::forward_as_tuple(std::forward<Us>(us)...)));
        }

        template<typename... Us>
        auto operator()(Us&&... us) &
        -> std::decay_t<std::invoke_result_t<std::decay_t<TFunc>&,
            std::decay_t<Ts>..., Us...>>
        {
            return std::apply(*func_, tuple_cat(*ts_,
                        std::make_tuple(std::forward<Us>(us)...)));
        }

        template<typename... Us>
        auto operator()(Us&&... us) &&
        -> std::decay_t<std::invoke_result_t<std::decay_t<TFunc>&&,
            std::decay_t<Ts>..., std::decay_t<Us>&&...>>
        {
            return std::apply(std::move(*func_), tuple_cat(std::move(*ts_),
                        std::make_tuple(std::forward<Us>(us)...)));
        }

    private:
        template <typename TFunc2, typename... Us>
        friend class Partial;

        ForceNoexcept<std::decay_t<TFunc>> func_;
        ForceNoexcept<std::tuple<std::decay_t<Ts>...>> ts_;
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

