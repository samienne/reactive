#pragma once

#include "constant.h"

#include <reactive/signal2.h>
#include <reactive/signaltraits.h>
#include <reactive/connection.h>

#include <btl/pipable.h>
#include <btl/option.h>
#include <btl/demangle.h>
#include <btl/apply.h>
#include <btl/typetraits.h>
#include <btl/tuplemap.h>
#include <btl/tuplereduce.h>
#include <btl/partial.h>
#include <btl/or.h>
#include <btl/plus.h>
#include <btl/tupleswitch.h>

#include <tuple>

namespace reactive
{
namespace signal
{
    namespace detail
    {
        enum class ChangedStatus
        {
            changed,
            unchanged,
            unknown
        };

        struct HasChanged
        {
            template <typename T>
            bool operator()(T const& t) const
            {
                return t.hasChanged();
            }
        };

        template <typename TFunc>
        struct Observe
        {
            Observe(typename std::decay<TFunc>::type const& func) :
                func_(func)
            {
            }

            template <typename T>
            Connection operator()(T&& t)
            {
                return std::forward<T>(t).observe(func_);
            }

            typename std::decay<TFunc>::type const& func_;
        };

        struct UpdateBegin
        {
            UpdateBegin(FrameInfo const& frame):
                frame_(frame)
            {
            }

            template <typename T>
            btl::option<signal_time_t> operator()(T&& t)
            {
                return std::forward<T>(t).updateBegin(frame_);
            }

            FrameInfo const& frame_;
        };

        struct UpdateEnd
        {
            UpdateEnd(FrameInfo const& frame):
                frame_(frame)
            {
            }

            template <typename T>
            btl::option<signal_time_t> operator()(T&& t)
            {
                return std::forward<T>(t).updateEnd(frame_);
            }

            FrameInfo const& frame_;
        };

        struct MinTime
        {
            btl::option<signal_time_t> operator()(
                    btl::option<signal_time_t> a,
                    btl::option<signal_time_t> b)
            {
                if (a.valid() && b.valid())
                    return btl::just(std::min(*a, *b));
                else if (a.valid())
                    return a;
                else if (b.valid())
                    return b;
                else
                    return btl::none;
            }
        };

        struct Evaluate
        {
            template <typename T>
            auto operator()(T&& obj)
            //-> std::decay_t<decltype(std::declval<T>().evaluate())>
            -> decltype(std::declval<T>().evaluate())
            {
                return (std::forward<T>(obj).evaluate());
            }
        };

        template <typename TFunc>
        struct Apply
        {
            typename std::decay<TFunc>::type& func_;

            Apply(typename std::decay<TFunc>::type& func):
                func_(func)
            {
            }

            template <typename... Ts>
            auto operator()(Ts&&... ts)
                -> typename std::decay
                <
                    decltype(std::declval<TFunc>()(ts...))
                >::type
            {
                return func_(std::forward<Ts>(ts)...);
            }
        };

        template <typename TFunc>
        struct ApplyPartialFunction
        {
            typename std::decay<TFunc>::type& func_;

            ApplyPartialFunction(typename std::decay<TFunc>::type& func):
                func_(func)
            {
            }

            template <typename... Ts>
            auto operator()(Ts&&... ts)
                -> decltype(btl::applyPartialFunction(
                            std::declval<TFunc>(), ts...))
            {
                return btl::applyPartialFunction(func_,
                        std::forward<Ts>(ts)...);
            }
        };

        template <typename TFunc>
        struct MapBase
        {
            using Apply = detail::Apply<TFunc>;
        };

        template <typename TFunc>
        struct MapFunction
        {
            using Apply = detail::ApplyPartialFunction<TFunc>;
        };
    } // detail

    template <template <typename> class TBase,
             typename TFunc, typename... TSigs>
    class Map final
    {
        using Base = TBase<TFunc>;
        using Apply = typename Base::Apply;
        using FuncType = typename std::decay<TFunc>::type;
        using FuncReturnType = std::result_of_t<Apply(SignalType<TSigs>...)>;

        // If all signal value types are references our evaluation type can
        // also be a reference. Otherwise we return a value type.
        using EvaluateType = std::conditional_t
            <
                btl::All<std::is_reference<SignalType<TSigs>>...>::value,
                FuncReturnType,
                std::decay_t<FuncReturnType>
            >;

    public:
        Map(TFunc func, TSigs... sigs) :
            func_(std::move(func)),
            signals_(std::make_tuple(std::move(sigs)...))
        {
        }

    private:
        Map(Map const&) = default;
        Map& operator=(Map const&) = default;

    public:
        Map(Map&&) = default;
        Map& operator=(Map&&) = default;

        EvaluateType evaluate() const
        {
            assert(ready_);
            return btl::apply(Apply(func_),
                    btl::tuple_map(*signals_, detail::Evaluate()));
        }

        bool hasChanged() const
        {
            assert(ready_);
            if (changed_ == detail::ChangedStatus::unknown)
            {
                bool changed = btl::tuple_reduce(false,
                        btl::tuple_map(*signals_, detail::HasChanged()),
                        [](bool current, bool next)
                        {
                            return current || next;
                        }
                        );

                changed_ = changed ? detail::ChangedStatus::changed :
                    detail::ChangedStatus::unchanged;

                return changed;
            }

            return changed_ == detail::ChangedStatus::changed;
        }

        btl::option<signal_time_t> updateBegin(FrameInfo const& frame)
        {
            auto r = btl::tuple_reduce(
                    btl::none,
                    btl::tuple_map(*signals_, detail::UpdateBegin(frame)),
                    detail::MinTime());

            ready_ = false;
            return r;
        }

        btl::option<signal_time_t> updateEnd(FrameInfo const& frame)
        {
            ready_ = true;
            auto r = btl::tuple_reduce(
                    btl::none,
                    btl::tuple_map(*signals_, detail::UpdateEnd(frame)),
                    detail::MinTime());

            changed_ = detail::ChangedStatus::unknown;

            return r;
        }

        template <typename TCallback>
        Connection observe(TCallback&& callback)
        {
            return btl::tuple_reduce(
                    Connection(),
                    btl::tuple_map(*signals_,
                        detail::Observe<TCallback>(callback)),
                    btl::Plus());
        }

        Annotation annotateI(Annotation::Node, Annotation a) const
        {
            return a;
        }

        template <typename U, typename... Us>
        Annotation annotateI(Annotation::Node n, Annotation a,
                U const& u, Us const&... us) const
        {
            a.addTree(n, u.annotate());
            return annotateI(n, std::move(a), us...);
        }

        Annotation annotate() const
        {
            auto f = [this](typename std::decay<TSigs>::type const&... sigs)
            {
                Annotation a;
                auto n = a.addNode(
                        "map<" + std::to_string((int)sizeof...(TSigs)) + ">() -> "
                        + btl::demangle<EvaluateType>()
                        + " status: " + std::to_string((int)changed_)
                        + " changed: " + std::to_string(hasChanged())
                        + " : " + std::to_string(hasChanged()));
                return this->annotateI(n, std::move(a), sigs...);
            };

            return btl::apply(f, *signals_);
        }

        Map clone() const
        {
            return *this;
        }

    private:
        mutable FuncType func_;
        btl::CloneOnCopy<std::tuple<std::decay_t<TSigs>...>> signals_;
        mutable detail::ChangedStatus changed_ = detail::ChangedStatus::unknown;
        mutable bool ready_ = true;
    };

    template <typename TFunc, typename... Ts, typename... Us, typename = std::enable_if_t
        <
            btl::All<
                std::is_copy_constructible<std::decay_t<TFunc>>,
                //IsSignal<TSigs>...,
                btl::IsClonable<std::result_of_t<std::decay_t<TFunc>(
                        Ts...
                        )>>
            >::value
        >>
    constexpr auto map(TFunc&& func, signal2::Signal<Ts, Us>... sigs)
    {
        return signal2::wrap(
                Map<detail::MapBase, std::decay_t<TFunc>, signal2::Signal<Ts, Us>...>(
                std::forward<TFunc>(func),
                std::move(sigs)...
                ));
    }

    template <typename TFunc, typename... TSigs,
             typename = typename
        std::enable_if
        <
            btl::All<
                std::is_copy_constructible<std::decay_t<TFunc>>,
                IsSignal<TSigs>...
            >::value
        >::type>
    constexpr auto mapFunction(TFunc&& func, TSigs... sigs)
    {
        return signal2::wrap(
                Map<detail::MapFunction, std::decay_t<TFunc>, std::decay_t<TSigs>...>(
                    std::forward<TFunc>(func),
                    std::move(sigs)...
                ));
    }

    namespace detail
    {
        struct DoMap
        {
            template <typename... Ts>
            auto operator()(Ts&&... ts) const
                -> decltype(map(std::forward<Ts>(ts)...))
            {
                return map(std::forward<Ts>(ts)...);
            }
        };

        template <typename... Ts>
        auto fmap2(Ts&&... ts)
        -> decltype(
                btl::apply(DoMap(),
                    btl::tuple_switch(
                        std::forward_as_tuple(std::forward<Ts>(ts)...)
                        )
                    )
                )
        {
            return btl::apply(DoMap(),
                    btl::tuple_switch(
                        std::forward_as_tuple(std::forward<Ts>(ts)...)
                        )
                    );
        }
    } // detail

    template <typename TFunc, typename... TSigs,
             typename = typename
        std::enable_if
        <
            btl::All<
                std::is_copy_constructible<std::decay_t<TFunc>>,
                IsSignal<TSigs>...
            >::value
        >::type>
    constexpr auto mapPartial(TFunc func, TSigs... sigs)
        -> decltype(btl::applyPartialFunction(
                    detail::DoMap(),
                    std::move(func),
                    std::move(sigs)...))
    {
        return btl::applyPartialFunction(
                detail::DoMap(),
                std::move(func),
                std::move(sigs)...
                );
    }

    static constexpr auto fmap2 = BTL_PIPABLE(detail::fmap2);
} // signal
} // reactive

namespace btl
{
    template <typename... Ts>
    auto fmap(Ts&&... ts)
    -> decltype(
            reactive::signal::map(std::forward<Ts>(ts)...)
        )
    {
        return reactive::signal::map(std::forward<Ts>(ts)...);
    }
} // btl

