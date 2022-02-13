#pragma once

#include "constant.h"
#include "signaltraits.h"
#include "signal.h"

namespace reactive::signal
{
    template <typename TSignal>
    class Changed;

    template <typename TSignal>
    struct IsSignal<Changed<TSignal>> : std::true_type {};

    template <typename TSignal>
    class Changed
    {
    public:
        Changed(Changed&&) noexcept = default;
        Changed& operator=(Changed&&) noexcept = default;

        Changed(TSignal sig):
            signal_(std::move(sig))
        {
        }

        bool evaluate() const
        {
            return changed_;
        }

        bool hasChanged() const
        {
            return changed_ != changedPrevious_;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            changedPrevious_ = changed_;
            return signal_.updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = signal_.updateEnd(frame);

            changed_ = signal_.hasChanged();
            if (changed_)
                return std::make_optional(signal_time_t(0));

            return r;
        }

        template <typename TFunc>
        Connection observe(TFunc&& f)
        {
            return signal_.observe(std::forward<TFunc>(f));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("changed()");
            a.addTree(n, signal_.annotate());
            return a;
        }

        Changed clone() const
        {
            return *this;
        }

    private:
        Changed(Changed const&) = default;
        Changed& operator=(Changed const&) = default;

    private:
        TSignal signal_;
        bool changedPrevious_ = false;
        bool changed_ = false;
    };

    template <typename T, typename TSignal>
    auto changed(Signal<TSignal, T> sig)
    {
        return wrap(Changed<Signal<std::decay_t<TSignal>, T>>(std::move(sig)));
    }
} // namespace reactive::signal

