#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class DropRepeats
        {
        public:
            DropRepeats(TSignal signal) :
                signal_(std::move(signal))
            {
            }

            DropRepeats(DropRepeats&&) = default;
            DropRepeats& operator=(DropRepeats&&) = default;

            signal_value_t<TSignal> const& evaluate() const
            {
                if (!value_.valid())
                    value_ = btl::just(btl::clone(signal_->evaluate()));

                return *value_;
            }

            bool hasChanged() const
            {
                return changed_;
            }

            UpdateResult updateBegin(FrameInfo const& frame)
            {
                return signal_->updateBegin(frame);
            }

            UpdateResult updateEnd(FrameInfo const& frame)
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
            Connection observe(TCallback&& callback)
            {
                return signal_->observe(std::forward<TCallback>(callback));
            }

            Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("dropRepeats() + changed: "
                        + std::to_string(hasChanged()));
                a.addTree(n, signal_->annotate());
                return a;
            }

            DropRepeats clone() const
            {
                return *this;
            }

        private:
            DropRepeats(DropRepeats const&) = default;
            DropRepeats& operator=(DropRepeats const&) = default;

        private:
            btl::CloneOnCopy<std::decay_t<TSignal>> signal_;
            mutable btl::option<signal_value_t<TSignal>> value_;
            bool changed_ = false;
        };

        static_assert(IsSignal<DropRepeats<Constant<int>>>::value,
                "DropRepeatsSignal is not a signal");

        template <typename TSignal, typename =
            std::enable_if_t
            <
                IsSignal<TSignal>::value
            >
        >
        auto dropRepeats(TSignal signal)
            -> DropRepeats<std::decay_t<TSignal>>
        {
            return DropRepeats<std::decay_t<TSignal>>(
                    std::move(signal));
        }

        template <typename TSignal, typename = typename
            std::enable_if
            <
                IsSignal<TSignal>::value
            >::type>
        auto tryDropRepeats(TSignal sig)
        -> decltype(dropRepeats(std::move(sig)))
        {
            return dropRepeats(std::move(sig));
        }
    }
}

