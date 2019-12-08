#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"
#include "reactive/reactivevisibility.h"

namespace reactive::signal
{
    template <typename TSignal>
    class CountSignal;

    template <typename TSignal>
    struct IsSignal<CountSignal<TSignal>> : std::true_type {};

    template <typename TSignal>
    class CountSignal
    {
    public:
        CountSignal(TSignal signal) :
            signal_(std::move(signal)),
            value_(0u)
        {
        }

        CountSignal(CountSignal&&) = default;
        CountSignal& operator=(CountSignal&&) = default;

        size_t evaluate() const
        {
            return std::abs(value_);
        }

        bool hasChanged() const
        {
            return value_ < 0;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return signal_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = signal_->updateEnd(frame);
            if (signal_->hasChanged())
                value_ = -std::abs(value_) - 1;

            return r;
        }

        template <typename TCallback>
        Connection observe(TCallback callback)
        {
            return signal_->observe(callback);
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("count()");
            a.addTree(n, signal_->annotate());
            return a;
        }

        CountSignal clone() const
        {
            return *this;
        }

    private:
        CountSignal(CountSignal const&) = default;
        CountSignal& operator=(CountSignal const&) = default;

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

