#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class BTL_CLASS_VISIBLE CountSignal;
    }

    template <typename TSignal>
    struct IsSignal<signal::CountSignal<TSignal>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename TSignal>
    class BTL_CLASS_VISIBLE CountSignal
    {
    public:
        BTL_HIDDEN CountSignal(TSignal signal) :
            signal_(std::move(signal)),
            value_(0u)
        {
        }

        BTL_HIDDEN CountSignal(CountSignal&&) = default;
        BTL_HIDDEN CountSignal& operator=(CountSignal&&) = default;

        BTL_HIDDEN size_t evaluate() const
        {
            return std::abs(value_);
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return value_ < 0;
        }

        BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
        {
            return signal_->updateBegin(frame);
        }

        BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = signal_->updateEnd(frame);
            if (signal_->hasChanged())
                value_ = -std::abs(value_) - 1;

            return r;
        }

        template <typename TCallback>
        BTL_HIDDEN Connection observe(TCallback callback)
        {
            return signal_->observe(callback);
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("count()");
            a.addTree(n, signal_->annotate());
            return a;
        }

        BTL_HIDDEN CountSignal clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN CountSignal(CountSignal const&) = default;
        BTL_HIDDEN CountSignal& operator=(CountSignal const&) = default;

    private:
        btl::CloneOnCopy<TSignal> signal_;
        int32_t value_;
    };

    static_assert(IsSignal<CountSignal<Constant<int>>>::value,
            "Count is not a signal");

    template <typename TSignal, typename = typename
        std::enable_if
        <
            IsSignal<TSignal>::value
        >::type>
    auto count(TSignal signal)
    {
        return CountSignal<std::decay_t<TSignal>>(std::move(signal));
    }

} // reactive::signal

BTL_VISIBILITY_POP

