#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace bq::signal::detail
{
    // TParams must be a value (non-reference) tuple: std::tuple_size_v and
    // std::tuple_element_t are ill-formed on a reference-to-tuple. The bound
    // values are matched as lvalues (&) to mirror std::apply over the lvalue
    // params tuple in BoundInvoker::operator().
    template <typename TFunc, typename TParams, typename... Us>
    struct IsBoundInvocable
    {
        template <std::size_t... Is>
        static constexpr bool check(std::index_sequence<Is...>)
        {
            return std::is_invocable_v<TFunc,
                    std::tuple_element_t<Is, TParams>&..., Us...>;
        }

        static constexpr bool value =
            check(std::make_index_sequence<std::tuple_size_v<TParams>>());
    };

    /**
     * @brief Applies a stored callable to bound leading values followed by the
     * arguments it is called with.
     *
     * Produced by Signal::bindFirst: the signal's values become the callable's
     * leading arguments and the call arguments follow them. The call operator
     * is SFINAE-constrained so a wrong-arity call is a clean substitution
     * failure rather than a hard error inside the body.
     */
    template <typename TFunc, typename TParams>
    class BoundInvoker
    {
    public:
        template <typename F, typename P>
        BoundInvoker(F&& func, P&& params) :
            func_(std::forward<F>(func)),
            params_(std::forward<P>(params))
        {
        }

        template <typename... Us, typename = std::enable_if_t<
            IsBoundInvocable<TFunc&, TParams, Us&&...>::value>>
        auto operator()(Us&&... us)
        {
            return std::apply([&](auto&&... ts) mutable
                    {
                        return func_(std::forward<decltype(ts)>(ts)...,
                                std::forward<Us>(us)...);
                    },
                    params_);
        }

    private:
        TFunc func_;
        TParams params_;
    };

    /**
     * @brief Makes a BoundInvoker, decaying the callable and the params tuple.
     */
    template <typename TFunc, typename TParams>
    auto makeBoundInvoker(TFunc&& func, TParams&& params)
    {
        return BoundInvoker<std::decay_t<TFunc>, std::decay_t<TParams>>(
                std::forward<TFunc>(func), std::forward<TParams>(params));
    }
} // namespace bq::signal::detail
