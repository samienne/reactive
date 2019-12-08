#pragma once

#include "constant.h"
#include "signaltraits.h"
#include "signal.h"

#include <btl/spinlock.h>
#include <btl/hidden.h>

namespace reactive::signal
{
    template <typename TStorage>
    class Cache;

    template <typename TStorage>
    struct IsSignal<Cache<TStorage>> : std::true_type {};

    template <typename TStorage>
    class Cache
    {
    public:
        using ValueType = std::decay_t<SignalType<TStorage>>;
        using Lock = std::lock_guard<btl::SpinLock>;

        Cache(TStorage sig) :
            sig_(std::move(sig))
        {
        }

    private:
        Cache(Cache const&) = default;
        Cache& operator=(Cache const&) = default;

    public:
        Cache(Cache&&) noexcept = default;
        Cache& operator=(Cache&&) noexcept = default;

        ValueType const& evaluate() const
        {
            if (value_.empty())
                value_ = btl::just(btl::clone(sig_->evaluate()));

            return *value_;
        }

        bool hasChanged() const
        {
            return changed_;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return sig_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = sig_->updateEnd(frame);

            changed_ = sig_->hasChanged();
            if (changed_)
                value_ = btl::none;

            return r;
        }

        template <typename TFunc>
        Connection observe(TFunc&& callback)
        {
            return sig_->observe(std::forward<TFunc>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("cache() changed: "
                    + std::to_string(hasChanged()));
            a.addTree(n, sig_->annotate());
            return a;
        }

        Cache clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<std::decay_t<TStorage>> sig_;
        mutable btl::option<ValueType> value_;
        bool changed_ = false;
    };

    template <typename T, typename U, std::enable_if_t<
        std::is_reference<SignalType<U>>::value
        , int> = 0>
    auto cache(Signal<U, T>&& sig)
    {
        return std::move(sig);
    }

    template <typename T, typename U, std::enable_if_t<
        !std::is_reference<SignalType<U>>::value
        , int> = 0>
    auto cache(Signal<U, T>&& sig)
    {
        return wrap(Cache<U>(std::move(sig).storage()));
    }

    template <typename T>
    auto cache(Signal<void, T>&& sig) -> AnySignal<T>
    {
        if (sig.isCached())
            return std::move(sig);
        else
        {
            return wrap(Cache<AnySignal<T>>(
                        std::move(sig))
                    );
        }
    }
} // namespace reactive::signal

