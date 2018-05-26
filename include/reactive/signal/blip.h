#pragma once

#include "constant.h"

#include "reactive/signaltraits.h"

#include <btl/option.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class BTL_CLASS_VISIBLE Blip;
    }

    template <typename TSignal>
    struct IsSignal<signal::Blip<TSignal>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename TSignal>
    class BTL_CLASS_VISIBLE Blip
    {
    public:
        BTL_HIDDEN Blip(Blip&&) = default;
        BTL_HIDDEN Blip& operator=(Blip&&) = default;

        BTL_HIDDEN Blip(TSignal sig) :
            sig_(std::move(sig))
        {
        }

        BTL_HIDDEN btl::option<signal_value_t<TSignal>> evaluate() const
        {
            if (!didChange_)
                return btl::none;

            return btl::just(btl::clone(sig_->evaluate()));
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return hadValue_ || sig_->hasChanged();
        }

        BTL_HIDDEN UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            auto r = sig_->updateBegin(frame);

            if (didChange_)
                r = btl::just(signal_time_t(0));

            return r;
        }

        BTL_HIDDEN UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            auto r = sig_->updateEnd(frame);

            hadValue_ = didChange_;
            didChange_ = sig_->hasChanged();

            return r;
        }

        template <typename TCallback>
        BTL_HIDDEN Connection observe(TCallback&& callback) noexcept
        {
            return sig_->observe(std::forward<TCallback>(callback));
        }

        BTL_HIDDEN Annotation annotate() const noexcept
        {
            Annotation a;
            auto&& n = a.addNode("blip()");
            a.addTree(n, sig_->annotate());
            return a;
        }

        BTL_HIDDEN Blip clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN Blip(Blip const&) = default;
        BTL_HIDDEN Blip& operator=(Blip const&) = default;

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
    auto blip(TSignal signal)
    {
        return Blip<std::decay_t<TSignal>>(std::move(signal));
    }
} // namespace reactive::signal

BTL_VISIBILITY_POP

