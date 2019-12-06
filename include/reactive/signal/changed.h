#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"
#include "reactive/reactivevisibility.h"

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class Changed;
    }

    template <typename TSignal>
    struct IsSignal<signal::Changed<TSignal>> : std::true_type {};
}

namespace reactive::signal
{
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
            return changed_ != changedPrevious_;
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
                return btl::just(signal_time_t(0));

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
        -> Changed<Signal<std::decay_t<TSignal>, T>>
    {
        return Changed<Signal<std::decay_t<TSignal>, T>>(std::move(sig));
    }
} // namespace reactive::signal

