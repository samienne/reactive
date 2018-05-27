#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"
#include "reactive/signal.h"

#include <btl/demangle.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename T, typename U>
        class BTL_CLASS_VISIBLE Delay;
    }

    template <typename T, typename U>
    struct IsSignal<signal::Delay<T, U>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename T, typename U>
    class BTL_CLASS_VISIBLE Delay
    {
    public:
        BTL_HIDDEN Delay(Signal<T, U>&& signal) :
            signal_(std::move(signal)),
            values_({
                    btl::just(btl::clone(signal_->evaluate())),
                    btl::none
                    })
        {
        }

        BTL_HIDDEN Delay(Delay&&) = default;
        BTL_HIDDEN Delay& operator=(Delay&&) = default;

        BTL_HIDDEN std::decay_t<T> const& evaluate() const
        {
            return *values_[index_];
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return changed_;
        }

        BTL_HIDDEN UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            auto r = signal_->updateBegin(frame);

            changed_ = false;
            if (values_[(index_+1)%2].valid())
            {
                index_ = (index_+1)%2;
                changed_ = true;
            }

            if (changed_)
                return btl::just(signal_time_t(0));

            return r;
        }

        BTL_HIDDEN UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            auto r = signal_->updateEnd(frame);
            if (signal_->hasChanged())
                values_[(index_+1)%2] = btl::just(btl::clone(signal_->evaluate()));
            else
                values_[(index_+1)%2] = btl::none;

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
            auto&& n = a.addNode("delay<" + btl::demangle<U>() + ">");
            a.addTree(n, signal_->annotate());
            return a;
        }

        BTL_HIDDEN Delay clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN Delay(Delay const&) = default;
        BTL_HIDDEN Delay& operator=(Delay const&) = default;

    private:
        btl::CloneOnCopy<Signal<T, U>> signal_;
        btl::option<std::decay_t<T>> values_[2];
        uint8_t index_ = 0;
        bool changed_ = false;
    };

    static_assert(IsSignal<Delay<int const&, signal::Constant<int>>>::value,
            "");

    template <typename T, typename U>
    auto delay(Signal<T, U> sig)
    {
        return wrap(Delay<T, U>{ std::move(sig) });
    }
} // namespace reactive::signal

BTL_VISIBILITY_POP

