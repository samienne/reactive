#pragma once

#include "reactive/signal.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class BTL_CLASS_VISIBLE Join
        {
        public:
            using OuterSig = typename std::decay<TSignal>::type;
            using InnerSig = decltype(std::declval<OuterSig>().evaluate());
            using SignalType = decltype(std::declval<InnerSig>().evaluate());

            BTL_HIDDEN Join(TSignal sig) :
                outer_(std::move(sig))
            {
            }

        private:
            BTL_HIDDEN Join(Join const&) = default;
            BTL_HIDDEN Join& operator=(Join const&) = default;

        public:
            BTL_HIDDEN Join(Join&&) noexcept = default;
            BTL_HIDDEN Join& operator=(Join&&) noexcept = default;

            BTL_HIDDEN btl::option<signal_time_t> updateBegin(FrameInfo const& frame)
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

            BTL_HIDDEN btl::option<signal_time_t> updateEnd(FrameInfo const& frame)
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

            BTL_HIDDEN SignalType evaluate() const
            {
                if (!inner_.valid())
                {
                    inner_ = btl::just(btl::cloneOnCopy(
                                btl::clone(outer_->evaluate())
                                ));
                }

                return (*inner_)->evaluate();
            }

            BTL_HIDDEN bool hasChanged() const
            {
                //return outer_.hasChanged() || inner_.hasChanged();
                return changed_;
            }

            template <typename TCallback>
            BTL_HIDDEN Connection observe(TCallback const& cb)
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

            BTL_HIDDEN Annotation annotate() const
            {
                Annotation a;
                auto n = a.addNode("join() changed: "
                        + std::to_string(hasChanged()));
                a.addTree(n, outer_->annotate());
                a.addTree(n, (*inner_)->annotate());
                return a;
            }

            BTL_HIDDEN Join clone() const
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
        auto join(Signal<T, U> sig)
        {
            return signal::wrap(Join<U>(std::move(sig).signal()));
        }
    }
}

BTL_VISIBILITY_POP

