#pragma once

#include "constant.h"
#include "reactive/signal.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class BTL_CLASS_VISIBLE DropRepeats
        {
        public:
            BTL_HIDDEN DropRepeats(TSignal signal) :
                signal_(std::move(signal))
            {
            }

            BTL_HIDDEN DropRepeats(DropRepeats&&) noexcept = default;
            BTL_HIDDEN DropRepeats& operator=(DropRepeats&&) noexcept = default;

            BTL_HIDDEN signal_value_t<TSignal> const& evaluate() const
            {
                if (!value_.valid())
                    value_ = btl::just(btl::clone(signal_->evaluate()));

                return *value_;
            }

            BTL_HIDDEN bool hasChanged() const
            {
                return changed_;
            }

            BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
            {
                return signal_->updateBegin(frame);
            }

            BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
            {
                auto r = signal_->updateEnd(frame);

                auto value = btl::clone(signal_->evaluate());

                if (!value_.valid())
                {
                    value_ = btl::just(std::move(value));
                    changed_ = signal_->hasChanged();
                    return r;
                }

                changed_ = !(value == *value_);
                value_ = btl::just(std::move(value));

                return r;
            }

            template <typename TCallback>
            BTL_HIDDEN Connection observe(TCallback&& callback)
            {
                return signal_->observe(std::forward<TCallback>(callback));
            }

            BTL_HIDDEN Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("dropRepeats() + changed: "
                        + std::to_string(hasChanged()));
                a.addTree(n, signal_->annotate());
                return a;
            }

            BTL_HIDDEN DropRepeats clone() const
            {
                return *this;
            }

        private:
            BTL_HIDDEN DropRepeats(DropRepeats const&) = default;
            BTL_HIDDEN DropRepeats& operator=(DropRepeats const&) = default;

        private:
            btl::CloneOnCopy<std::decay_t<TSignal>> signal_;
            mutable btl::option<signal_value_t<TSignal>> value_;
            bool changed_ = false;
        };

        static_assert(IsSignal<DropRepeats<Constant<int>>>::value,
                "DropRepeatsSignal is not a signal");

        template <typename T, typename U>
        auto dropRepeats(Signal<T, U> signal)
        {
            return wrap(
                    DropRepeats<Signal<T, U>>(
                        std::move(signal))
                    );
        }

        template <typename T, typename U>
        auto tryDropRepeats(Signal<T, U> sig)
        -> decltype(dropRepeats(std::move(sig)))
        {
            return dropRepeats(std::move(sig));
        }
    }
}

BTL_VISIBILITY_POP

