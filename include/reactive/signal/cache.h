#pragma once

#include "constant.h"
#include "reactive/signaltype.h"
#include "reactive/signaltraits.h"
#include "reactive/signal2.h"

#include <btl/spinlock.h>

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class Cache
        {
        public:
            using ValueType = std::decay_t<SignalType<TSignal>>;
            using Lock = std::lock_guard<btl::SpinLock>;

            Cache(TSignal sig) :
                sig_(std::move(sig))
            {
            }

        private:
            Cache(Cache const&) = default;
            Cache& operator=(Cache const&) = default;

        public:
            Cache(Cache&&) = default;
            Cache& operator=(Cache&&) = default;

            ValueType const& evaluate() const
            {
                if (value_.empty())
                    value_ = btl::just(btl::clone(sig_->evaluate()));

                return *value_;
            }

            bool hasChanged() const
            {
                return changed_;
            }

            UpdateResult updateBegin(FrameInfo const& frame)
            {
                return sig_->updateBegin(frame);
            }

            UpdateResult updateEnd(FrameInfo const& frame)
            {
                auto r = sig_->updateEnd(frame);

                changed_ = sig_->hasChanged();
                if (changed_)
                    value_ = btl::none;

                return r;
            }

            template <typename TFunc>
            Connection observe(TFunc&& callback)
            {
                return sig_->observe(std::forward<TFunc>(callback));
            }

            Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("cache() changed: "
                        + std::to_string(hasChanged()));
                a.addTree(n, sig_->annotate());
                return a;
            }

            Cache clone() const
            {
                return *this;
            }

        private:
            btl::CloneOnCopy<std::decay_t<TSignal>> sig_;
            mutable btl::option<ValueType> value_;
            bool changed_ = false;
        };

        /*static_assert(IsSignal<Cache<Constant<int>>>::value,
                "Cache is not a signal");*/

        template <typename T, typename U>
        auto cache(signal2::Signal<T const&, U>&& sig)
        {
            return std::move(sig);
        }

        template <typename T, typename U>
        auto cache(signal2::Signal<T, U>&& sig)
        {
            return signal2::wrap(Cache<U>(std::move(sig).signal()));
        }
    } // signal
} // reactive

