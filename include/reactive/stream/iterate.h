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
#include <btl/apply.h>

#include <type_traits>

namespace reactive
{
    namespace stream
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

            signal_value_t<TInitial> evaluate() const
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
                    value_ = btl::just(btl::apply(
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
            mutable btl::option<signal_value_t<TInitial>> value_;
        };

        template <typename TFunc, typename T, typename TInitial,
                 typename... TSignals, typename =
                     std::enable_if_t<
                        btl::All<
                            AreSignals<TInitial, TSignals...>,
                            std::is_convertible<
                                std::result_of_t<TFunc(
                                    signal_value_t<TInitial>,
                                    T,
                                    signal_value_t<TSignals>...)
                                >,
                                signal_value_t<TInitial>
                            >
                        >::value
                     >>
        auto iterate(TFunc func, TInitial initial, Stream<T> stream,
                TSignals... signals)
        {
            return Iterate<
                std::decay_t<TFunc>,
                decltype(signal2::wrap(std::declval<
                            signal::DropRepeats<std::decay_t<TInitial>>>()
                            )),
                T,
                std::decay_t<TSignals>...
                >(
                    std::move(func),
                    signal::dropRepeats(std::move(initial)),
                    std::move(stream),
                    std::move(signals)...
                );
        }

        template <typename TFunc, typename T, typename TInitial,
                 typename... TSignals, typename =
                     std::enable_if_t<
                        !IsSignal<TInitial>::value
                    >,
                 int = 0
                >
        auto iterate(TFunc&& func, TInitial&& initial, Stream<T> stream,
                TSignals... signals)
        -> decltype(
             iterate(
                    std::forward<TFunc>(func),
                    signal::constant(std::forward<TInitial>(initial)),
                    std::move(stream),
                    std::move(signals)...
                    ))
        {
            return iterate(
                    std::forward<TFunc>(func),
                    signal::constant(std::forward<TInitial>(initial)),
                    std::move(stream),
                    std::move(signals)...
                    );
        }
    } // stream
} // reactive

