#pragma once

#include "constant.h"
#include "reactive/signal.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

#include <type_traits>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive::signal
{
    template <typename TCondition, typename TTrue, typename TFalse>
    class BTL_CLASS_VISIBLE Conditional
    {
    public:
        BTL_HIDDEN Conditional(TCondition condition, TTrue t, TFalse f) :
            condition_(std::move(condition)),
            t_(std::move(t)),
            f_(std::move(f))
        {
        }

        BTL_HIDDEN Conditional(Conditional&&) noexcept = default;
        BTL_HIDDEN Conditional& operator=(Conditional&&) noexcept = default;

        BTL_HIDDEN signal_value_t<TTrue> evaluate() const
        {
            if (condition_->evaluate())
                return btl::clone(t_->evaluate());
            else
                return btl::clone(f_->evaluate());
        }

        BTL_HIDDEN bool hasChanged() const
        {
            if (changed_)
                return true;

            if (state_)
                return t_->hasChanged();
            else
                return f_->hasChanged();
        }

        BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
        {
            auto r1 = condition_->updateBegin(frame);
            auto r2 = t_->updateBegin(frame);
            auto r3 = f_->updateBegin(frame);

            return min(r1, min(r2, r3));
        }

        BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = condition_->updateEnd(frame);
            auto r2 = r;
            if (state_)
                r2 = t_->updateEnd(frame);
            else
                r2 = f_->updateEnd(frame);

            r = min(r, r2);

            bool state = condition_->evaluate();
            if (state_ != state)
                changed_ = !first_;

            first_ = false;

            state_ = state;

            if (changed_ && state_)
                r2 = t_->updateEnd(frame);
            else
                r2 = f_->updateEnd(frame);

            return min(r, r2);
        }

        template <typename TCallback>
        BTL_HIDDEN Connection observe(TCallback&& callback)
        {
            auto c = condition_->observe(callback);
            c += t_->observe(callback);
            c += f_->observe(callback);

            return std::move(c);
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            a.addNode("Conditional");
            return a;
        }

        BTL_HIDDEN Conditional clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN Conditional(Conditional const&) = default;
        BTL_HIDDEN Conditional& operator=(Conditional const&) = default;

    private:
        btl::CloneOnCopy<std::decay_t<TCondition>> condition_;
        btl::CloneOnCopy<std::decay_t<TTrue>> t_;
        btl::CloneOnCopy<std::decay_t<TFalse>> f_;
        bool state_ = false;
        bool changed_ = false;
        bool first_ = true;
    };

    static_assert(IsSignal<Conditional<Constant<bool>, Constant<int>,
            Constant<int>>>::value, "");


    template <typename TCondition, typename TTrue, typename TFalse,
                typename = std::enable_if_t
                <
                btl::All<
                    IsSignal<TCondition>,
                    IsSignal<TTrue>,
                    IsSignal<TFalse>,
                    std::is_same<bool, signal_value_t<TCondition>>,
                    std::is_same<signal_value_t<TTrue>,
                        signal_value_t<TFalse>>
                >::value
                >>
    auto conditional(TCondition condition, TTrue t, TFalse f)
        //-> Conditional<TCondition, TTrue, TFalse>
    {
        return wrap(Conditional<
            std::decay_t<TCondition>,
            std::decay_t<TTrue>,
            std::decay_t<TFalse>
                >(
                std::move(condition),
                std::move(t),
                std::move(f)
                ));
    }
} // reactive::signal

BTL_VISIBILITY_POP

