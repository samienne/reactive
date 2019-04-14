#pragma once

#include "tee.h"
#include "map.h"
#include "input.h"

#include "reactive/reactivevisibility.h"

#include <btl/apply.h>
#include <btl/hidden.h>

#include <utility>

namespace reactive
{
    namespace signal
    {
        template <typename TFunc, typename TValue, typename TSignal>
        class FoldP;
    }

    template <typename TFunc, typename TValue, typename TSignal>
    struct IsSignal<signal::FoldP<TFunc, TValue, TSignal>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename TFunc, typename TValue, typename TSignal>
    class FoldP
    {
    public:
        using ValueType = typename std::decay<TValue>::type;

        //template <typename T>
        FoldP(TFunc func, TValue initial, TSignal sig) :
            sig_(std::move(sig)),
            func_(std::move(func)),
            value_(std::move(initial))
        {
        }

        FoldP(FoldP&&) = default;
        FoldP& operator=(FoldP&&) = default;

        ValueType const& evaluate() const
        {
            return value_;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return sig_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = sig_->updateEnd(frame);

            if (sig_->hasChanged())
                value_ = func_(std::move(value_), sig_->evaluate());

            return r;
        }

        bool hasChanged() const
        {
            return sig_->hasChanged();
        }

        template <typename TCallback>
        Connection observe(TCallback&& cb)
        {
            return sig_->observe(std::forward<TCallback>(cb));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("foldp()");
            a.addTree(n, sig_->annotate());
            return a;
        }

        FoldP clone() const
        {
            return *this;
        }

    private:
        FoldP(FoldP const&) = default;
        FoldP& operator=(FoldP const&) = default;

    private:
        btl::CloneOnCopy<std::decay_t<TSignal>> sig_;
        std::decay_t<TFunc> func_;
        std::decay_t<TValue> value_;
    };

    template <typename TFunc, typename TValue, typename TSignal,
            typename = typename std::enable_if
        <
            IsSignal<TSignal>::value &&
            std::is_same<
                typename std::decay<TValue>::type,
                typename std::decay<
                    decltype(
                            std::declval<TFunc>()(
                                std::declval<TValue>(),
                                std::declval<TSignal>().evaluate())
                            )>::type
                    >::value
        >::type
        >
    auto foldp(TFunc func, TValue initial, TSignal sig)
        /*-> FoldP<
            std::decay_t<TFunc>,
            std::decay_t<TValue>,
            std::decay_t<TSignal>
            >*/
    {
        return wrap(FoldP<
            std::decay_t<TFunc>,
            std::decay_t<TValue>,
            std::decay_t<TSignal>>
                (
                    std::move(func),
                    std::move(initial),
                    std::move(sig)
                ));
    }

    namespace detail
    {
        struct Tupler
        {
            template <typename... Ts>
            std::tuple<btl::decay_t<Ts>...> operator()(Ts&&... ts) const
            {
                return std::make_tuple(std::forward<Ts>(ts)...);
            }
        };

        template <typename TFunc>
        struct TupleCaller
        {
            template <typename T, typename TTuple>
            auto operator()(T&& value, TTuple&& tuple) const
            -> decltype(
                std::apply(
                        std::declval<TFunc>(),
                        std::tuple_cat(
                            std::forward_as_tuple(std::forward<T>(value)),
                            std::forward<TTuple>(tuple)
                            )
                        )
                )
            {
                return std::apply(
                        func_,
                        std::tuple_cat(
                            std::forward_as_tuple(std::forward<T>(value)),
                            std::forward<TTuple>(tuple)
                            )
                        );
            }

            btl::decay_t<TFunc> func_;
        };
    } // detail

    template <typename TFunc, typename TValue, typename TSignal1,
                typename TSignal2, typename... TSignals>
    auto foldp(TFunc&& func, TValue&& initial, TSignal1&& signal1,
            TSignal2&& signal2, TSignals&&... signals)
    -> decltype(
            foldp(
                detail::TupleCaller<TFunc>{std::forward<TFunc>(func)},
                std::forward<TValue>(initial),
                map(detail::Tupler(),
                    std::forward<TSignal1>(signal1),
                    std::forward<TSignal2>(signal2),
                    std::forward<TSignals>(signals)...
                    )
            )
        )
    {
        return foldp(
                detail::TupleCaller<TFunc>{std::forward<TFunc>(func)},
                std::forward<TValue>(initial),
                map(detail::Tupler(),
                    std::forward<TSignal1>(signal1),
                    std::forward<TSignal2>(signal2),
                    std::forward<TSignals>(signals)...
                    )
                );
    }
} // reactive

