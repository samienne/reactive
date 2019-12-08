#pragma once

#include "collect.h"
#include "stream.h"

#include "reactive/signal/tee.h"
#include "reactive/signal/input.h"
#include "reactive/signal/droprepeats.h"
#include "reactive/signal/blip.h"

#include "reactive/signaltraits.h"

#include <btl/tuplemap.h>
#include <btl/tupleforeach.h>

#include <type_traits>

namespace reactive
{
    namespace stream
    {
        template <typename TFunc, typename TInitial, typename T,
                    typename... TSignals>
        class Iterate;

        template <typename TFunc, typename TInitial, typename T,
                    typename... TSignals>
        class IterateStatic;
    }

    namespace signal
    {
        template <typename TFunc, typename TInitial, typename T, typename... TSignals>
        struct IsSignal<stream::Iterate<TFunc, TInitial, T, TSignals...>>
        : std::true_type {};

        template <typename TFunc, typename TInitial, typename T, typename... TSignals>
        struct IsSignal<stream::IterateStatic<TFunc, TInitial, T, TSignals...>>
        : std::true_type {};
    }
}

namespace reactive::stream
{
    namespace detail
    {
        struct Evaluate
        {
            template <typename T>
            auto operator()(T&& t)
                -> decltype(t.evaluate())
            {
                return std::forward<T>(t).evaluate();
            }
        };

        struct UpdateBegin
        {
            template <typename T>
            void operator()(T&& t)
            {
                std::forward<T>(t).updateBegin(frame);
            }

            signal::FrameInfo const& frame;
        };

        struct UpdateEnd
        {
            template <typename T>
            void operator()(T&& t)
            {
                std::forward<T>(t).updateEnd(frame);
            }

            signal::FrameInfo const& frame;
        };
    } // detail

    template <typename TFunc, typename TInitial, typename T,
                typename... TSignals>
    class Iterate
    {
    public:
        Iterate(TFunc func, TInitial initial, Stream<T> stream,
                TSignals... sigs) :
            func_(std::move(func)),
            sigs_(std::make_tuple(std::move(sigs)...)),
            initial_(std::move(initial)),
            streamValues_(collect(std::move(stream)))
        {
        }

    private:
        Iterate(Iterate const&) = default;
        Iterate& operator=(Iterate const&) = default;

    public:
        Iterate(Iterate&&) = default;
        Iterate& operator=(Iterate&&) = default;

        signal::signal_value_t<TInitial> evaluate() const
        {
            if (!value_.valid())
                value_ = btl::just(btl::clone(initial_->evaluate()));

            return *value_;
        }

        signal::UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            auto r = initial_->updateBegin(frame);
            btl::tuple_foreach(*sigs_, stream::detail::UpdateBegin{frame});
            streamValues_->updateBegin(frame);

            return r;
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            auto r = initial_->updateEnd(frame);
            btl::tuple_foreach(*sigs_, stream::detail::UpdateEnd{frame});
            auto r2 = streamValues_->updateEnd(frame);

            if (!value_.valid() || initial_->hasChanged())
            {
                value_ = btl::clone(btl::just(initial_->evaluate()));
            }

            for (auto&& v : streamValues_->evaluate())
            {
                value_ = btl::just(std::apply(
                        func_,
                        std::tuple_cat(
                            std::make_tuple(std::move(*value_),
                                std::forward<decltype(v)>(v)
                                ),
                            btl::tuple_map(*sigs_, stream::detail::Evaluate())
                            )
                        ));
            }

            return min(r, r2);
        }

        bool hasChanged() const
        {
            return streamValues_->hasChanged() || initial_->hasChanged();
        }

        template <typename TCallback>
        Connection observe(TCallback&& cb)
        {
            return initial_->observe(cb)
                + streamValues_->observe(std::forward<TCallback>(cb));
        }

        Annotation annotate() const
        {
            Annotation a;
            //auto&& n = a.addNode("iterate()");
            //a.addTree(n, sig_.annotate());
            return a;
        }

        Iterate clone() const
        {
            return *this;
        }

    private:
        std::decay_t<TFunc> func_;
        btl::CloneOnCopy<std::tuple<std::decay_t<TSignals>...>> sigs_;
        btl::CloneOnCopy<std::decay_t<TInitial>> initial_;
        btl::CloneOnCopy<decltype(collect(std::declval<Stream<T>>()))> streamValues_;
        mutable btl::option<signal::signal_value_t<TInitial>> value_;
    };

    template <typename TFunc, typename TInitial, typename T,
                typename... TSignals>
    class IterateStatic
    {
    public:
        IterateStatic(TFunc func, TInitial initial, Stream<T> stream,
                TSignals... sigs) :
            func_(std::move(func)),
            sigs_(std::make_tuple(std::move(sigs)...)),
            value_(std::move(initial)),
            streamValues_(collect(std::move(stream)))
        {
        }

    private:
        IterateStatic(IterateStatic const&) = default;
        IterateStatic& operator=(IterateStatic const&) = default;

    public:
        IterateStatic(IterateStatic&&) = default;
        IterateStatic& operator=(IterateStatic&&) = default;

        TInitial const& evaluate() const
        {
            return value_;
        }

        signal::UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            btl::tuple_foreach(*sigs_, stream::detail::UpdateBegin{frame});
            streamValues_->updateBegin(frame);

            return btl::none;
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            btl::tuple_foreach(*sigs_, stream::detail::UpdateEnd{frame});
            auto r = streamValues_->updateEnd(frame);

            for (auto&& v : streamValues_->evaluate())
            {
                value_ = std::apply(
                        func_,
                        std::tuple_cat(
                            std::make_tuple(std::move(value_),
                                std::forward<decltype(v)>(v)
                                ),
                            btl::tuple_map(*sigs_, stream::detail::Evaluate())
                            )
                        );
            }

            return r;
        }

        bool hasChanged() const
        {
            return streamValues_->hasChanged();
        }

        template <typename TCallback>
        Connection observe(TCallback&& cb)
        {
            return streamValues_->observe(std::forward<TCallback>(cb));
        }

        Annotation annotate() const
        {
            Annotation a;
            //auto&& n = a.addNode("iterate()");
            //a.addTree(n, sig_.annotate());
            return a;
        }

        IterateStatic clone() const
        {
            return *this;
        }

    private:
        std::decay_t<TFunc> func_;
        btl::CloneOnCopy<std::tuple<std::decay_t<TSignals>...>> sigs_;
        std::decay_t<TInitial> value_;
        btl::CloneOnCopy<decltype(collect(std::declval<Stream<T>>()))> streamValues_;
    };

    template <typename TFunc, typename T, typename TInitial,
                typename... TSignals, typename =
                    std::enable_if_t<
                    btl::All<
                        signal::AreSignals<TInitial, TSignals...>,
                        std::is_convertible<
                            std::result_of_t<TFunc(
                                signal::signal_value_t<TInitial>,
                                T,
                                signal::signal_value_t<TSignals>...)
                            >,
                            signal::signal_value_t<TInitial>
                        >
                    >::value
                    >>
    auto iterate(TFunc func, TInitial initial, Stream<T> stream,
            TSignals... signals)
    {
        return signal::wrap(Iterate<
            std::decay_t<TFunc>,
            decltype(signal::wrap(signal::dropRepeats(std::declval<TInitial>()))),
            T,
            std::decay_t<TSignals>...
            >(
                std::move(func),
                signal::dropRepeats(std::move(initial)),
                std::move(stream),
                std::move(signals)...
            ));
    }

    template <typename TFunc, typename T, typename TInitial,
                typename... TSignals, typename =
                    std::enable_if_t<
                    btl::All<
                        signal::AreSignals<TSignals...>,
                        btl::Not<signal::IsSignal<TInitial>>,
                        std::is_convertible<
                            std::result_of_t<TFunc(
                                TInitial,
                                T,
                                signal::signal_value_t<TSignals>...)
                            >,
                            TInitial
                        >
                    >::value
                    >,
                    int = 0>
    auto iterate(TFunc func, TInitial initial, Stream<T> stream,
            TSignals... signals)
    {
        return signal::wrap(IterateStatic<
            std::decay_t<TFunc>,
            std::decay_t<TInitial>,
            T,
            std::decay_t<TSignals>...
            >(
                std::move(func),
                std::move(initial),
                std::move(stream),
                std::move(signals)...
            ));
    }
} // namespace reactive::stream

