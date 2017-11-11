#pragma once

#include <reactive/sharedsignal.h>
#include <reactive/signal.h>
#include <reactive/signaltraits.h>

#include <btl/tuplemap.h>
#include <btl/reduce.h>
#include <btl/fmap.h>
#include <btl/foreach.h>
#include <btl/hidden.h>

#include <vector>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignals>
        class BTL_CLASS_VISIBLE Combine
        {
        public:
            BTL_HIDDEN Combine(TSignals signals) :
                sigs_(std::move(signals))
            {
            }

        private:
            BTL_HIDDEN Combine(Combine const&) = default;
            BTL_HIDDEN Combine& operator=(Combine const&) = default;

        public:
            BTL_HIDDEN Combine(Combine&&) noexcept(true) = default;
            BTL_HIDDEN Combine& operator=(Combine&&) noexcept(true) = default;

            BTL_HIDDEN auto evaluate() const
            {
                return btl::fmap(*sigs_, [](auto&& sig)
                        {
                            return btl::clone(sig.evaluate());
                        });
            }

            BTL_HIDDEN bool hasChanged() const
            {
                return btl::reduce(false, *sigs_,
                        [](bool initial, auto const& sig) noexcept
                        {
                            return initial || sig.hasChanged();
                        });
            }

            BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
            {
                return btl::reduce(
                        btl::option<signal_time_t>(btl::none), *sigs_,
                        [&frame](btl::option<signal_time_t> t, auto& sig) noexcept
                        {
                            auto t2 = sig.updateBegin(frame);
                            return min(t, t2);
                        });
            }

            BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
            {
                return btl::reduce(
                        btl::option<signal_time_t>(btl::none), *sigs_,
                        [&frame](btl::option<signal_time_t> t, auto& sig) noexcept
                        {
                            auto t2 = sig.updateEnd(frame);
                            return min(t, t2);
                        });
            }

            template <typename TCallback>
            BTL_HIDDEN Connection observe(TCallback&& callback) noexcept
            {
                return btl::reduce(Connection(), *sigs_,
                        [callback = std::forward<TCallback>(callback)]
                        (Connection c, auto& sig) noexcept
                        {
                            return std::move(c) + sig.observe(callback);
                        });
            }

            BTL_HIDDEN Annotation annotate() const noexcept
            {
                Annotation a;
                auto n = a.addNode("combine()");

                btl::forEach(*sigs_, [&a, &n](auto&& sig)
                    {
                        a.addTree(n, sig.annotate());
                    });

                return a;
            }

            BTL_HIDDEN Combine clone() const
            {
                return *this;
            }

        private:
            btl::CloneOnCopy<btl::decay_t<TSignals>> sigs_;
        };

        template <typename T, typename = std::enable_if_t<
            IsSignal<T>::value
            >
        >
        auto combine(std::vector<T> signals)
            ///-> Combine<std::vector<T>>
        {
            return signal::wrap(Combine<std::vector<T>>(std::move(signals)));
        }

        template <typename... Ts>
        auto combine(std::tuple<Ts...> signals)
            //-> Combine<std::tuple<Ts...>>
        {
            return signal::wrap(Combine<std::tuple<Ts...>>(std::move(signals)));
        }
    } // signal
} // reactive

BTL_VISIBILITY_POP

