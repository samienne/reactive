#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"
#include "reactive/signal.h"

#include <btl/spinlock.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class BTL_CLASS_VISIBLE Cache;
    }

    template <typename TSignal>
    struct IsSignal<signal::Cache<TSignal>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename TSignal>
    class BTL_CLASS_VISIBLE Cache
    {
    public:
        using ValueType = std::decay_t<SignalType<TSignal>>;
        using Lock = std::lock_guard<btl::SpinLock>;

        BTL_HIDDEN Cache(TSignal sig) :
            sig_(std::move(sig))
        {
        }

    private:
        BTL_HIDDEN Cache(Cache const&) = default;
        BTL_HIDDEN Cache& operator=(Cache const&) = default;

    public:
        BTL_HIDDEN Cache(Cache&&) noexcept = default;
        BTL_HIDDEN Cache& operator=(Cache&&) noexcept = default;

        BTL_HIDDEN ValueType const& evaluate() const
        {
            if (value_.empty())
                value_ = btl::just(btl::clone(sig_->evaluate()));

            return *value_;
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return changed_;
        }

        BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
        {
            return sig_->updateBegin(frame);
        }

        BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = sig_->updateEnd(frame);

            changed_ = sig_->hasChanged();
            if (changed_)
                value_ = btl::none;

            return r;
        }

        template <typename TFunc>
        BTL_HIDDEN Connection observe(TFunc&& callback)
        {
            return sig_->observe(std::forward<TFunc>(callback));
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("cache() changed: "
                    + std::to_string(hasChanged()));
            a.addTree(n, sig_->annotate());
            return a;
        }

        BTL_HIDDEN Cache clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<std::decay_t<TSignal>> sig_;
        mutable btl::option<ValueType> value_;
        bool changed_ = false;
    };

    template <typename T, typename U, std::enable_if_t<
        std::is_reference<SignalType<U>>::value
        , int> = 0>
    auto cache(Signal<T, U>&& sig)
    {
        return std::move(sig);
    }

    template <typename T, typename U, std::enable_if_t<
        !std::is_reference<SignalType<U>>::value
        , int> = 0>
    auto cache(Signal<T, U>&& sig)
    {
        return signal::wrap(Cache<U>(std::move(sig).signal()));
    }

    template <typename T>
    auto cache(Signal<T, void>&& sig) -> Signal<T, void>
    {
        if (sig.isCached())
            return std::move(sig);
        else
        {
            return signal::wrap(Cache<Signal<T, void>>(
                        std::move(sig))
                    );
        }
    }
} // namespace reactive::signal

BTL_VISIBILITY_POP

