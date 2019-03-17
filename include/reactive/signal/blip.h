#pragma once

#include "constant.h"

#include "reactive/signaltraits.h"

#include <btl/option.h>

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class Blip;
    }

    template <typename TSignal>
    struct IsSignal<signal::Blip<TSignal>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename TSignal>
    class Blip
    {
    public:
        Blip(Blip&&) = default;
        Blip& operator=(Blip&&) = default;

        Blip(TSignal sig) :
            sig_(std::move(sig))
        {
        }

        btl::option<signal_value_t<TSignal>> evaluate() const
        {
            if (!didChange_)
                return btl::none;

            return btl::just(btl::clone(sig_->evaluate()));
        }

        bool hasChanged() const
        {
            return hadValue_ || sig_->hasChanged();
        }

        UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            auto r = sig_->updateBegin(frame);

            if (didChange_)
                r = btl::just(signal_time_t(0));

            return r;
        }

        UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            auto r = sig_->updateEnd(frame);

            hadValue_ = didChange_;
            didChange_ = sig_->hasChanged();

            return r;
        }

        template <typename TCallback>
        Connection observe(TCallback&& callback) noexcept
        {
            return sig_->observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const noexcept
        {
            Annotation a;
            auto&& n = a.addNode("blip()");
            a.addTree(n, sig_->annotate());
            return a;
        }

        Blip clone() const
        {
            return *this;
        }

    private:
        Blip(Blip const&) = default;
        Blip& operator=(Blip const&) = default;

    private:
        btl::CloneOnCopy<std::decay_t<TSignal>> sig_;
        bool didChange_ = true;
        bool hadValue_ = false;
    };

    template <typename TSignal, typename = typename
        std::enable_if_t<
            IsSignal<TSignal>::value
            >
        >
    REACTIVE_EXPORT auto blip(TSignal signal)
    {
        return Blip<std::decay_t<TSignal>>(std::move(signal));
    }
} // namespace reactive::signal

