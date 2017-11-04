#pragma once

#include "reactive/signal2.h"
#include "reactive/signaltraits.h"


namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class Join
        {
        public:
            using OuterSig = typename std::decay<TSignal>::type;
            using InnerSig = decltype(std::declval<OuterSig>().evaluate());
            using SignalType = decltype(std::declval<InnerSig>().evaluate());

            Join(TSignal sig) :
                outer_(std::move(sig))
            {
            }

        private:
            Join(Join const&) = default;
            Join& operator=(Join const&) = default;

        public:
            Join(Join&&) noexcept = default;
            Join& operator=(Join&&) noexcept = default;

            btl::option<signal_time_t> updateBegin(FrameInfo const& frame)
            {
                changed_ = false;
                auto r = outer_->updateBegin(frame);
                if (inner_.valid())
                {
                    auto r2 = (*inner_)->updateBegin(frame);
                    r = min(r, r2);
                }

                return r;
            }

            btl::option<signal_time_t> updateEnd(FrameInfo const& frame)
            {
                auto r1 = outer_->updateEnd(frame);

                btl::option<signal_time_t> r2 = btl::none;
                if (!inner_.valid() || outer_->hasChanged())
                {
                    inner_ = btl::just(btl::cloneOnCopy(
                                btl::clone(outer_->evaluate())
                                ));

                    r2 = (*inner_)->updateBegin(frame);
                    auto r3 = (*inner_)->updateEnd(frame);
                    r2 = min(r2, r3),

                    changed_ = true;
                }
                else
                {
                    r2 = (*inner_)->updateEnd(frame);
                    changed_ = (*inner_)->hasChanged();
                }

                return min(r1, r2);
            }

            SignalType evaluate() const
            {
                if (!inner_.valid())
                {
                    inner_ = btl::just(btl::cloneOnCopy(
                                btl::clone(outer_->evaluate())
                                ));
                }

                return (*inner_)->evaluate();
            }

            bool hasChanged() const
            {
                //return outer_.hasChanged() || inner_.hasChanged();
                return changed_;
            }

            template <typename TCallback>
            Connection observe(TCallback const& cb)
            {
                if (!inner_.valid())
                {
                    inner_ = btl::just(btl::cloneOnCopy(
                                btl::clone(outer_->evaluate())
                                ));
                }

                auto c = outer_->observe(cb);
                c += (*inner_)->observe(cb);

                return std::move(c);
            }

            Annotation annotate() const
            {
                Annotation a;
                auto n = a.addNode("join() changed: "
                        + std::to_string(hasChanged()));
                a.addTree(n, outer_->annotate());
                a.addTree(n, (*inner_)->annotate());
                return a;
            }

            Join clone() const
            {
                return *this;
            }

        private:
            btl::CloneOnCopy<OuterSig> outer_;
            mutable btl::option<btl::CloneOnCopy<std::decay_t<InnerSig>>> inner_;
            bool changed_ = false;
        };

        template <typename T, typename U, typename = std::enable_if_t<
            IsSignal<T>::value
            >>
        auto join(signal2::Signal<T, U> sig)
        {
            return signal2::wrap(Join<U>(std::move(sig).signal()));
        }
    }
}

