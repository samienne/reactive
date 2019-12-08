#pragma once

#include "share.h"

namespace reactive::signal
{
    template <typename TDeferred, typename T>
    class Share;

    template <typename TStorage, typename T>
    class SharedSignal;

    template <typename TStorage, typename T>
    struct IsSignal<SharedSignal<TStorage, T>> : std::true_type {};

    template <typename TStorage, typename T>
    class SharedSignal : public Signal<
                         Share<Typed<TStorage, T>, T>, T
                         >
    {
    private:
        SharedSignal(Share<Typed<TStorage, T>, T> sig) :
            Signal<Share<Typed<TStorage, T>, T>, T>(std::move(sig))
        {
        }

    public:
        static SharedSignal create(Share<Typed<TStorage, T>, T>&& sig)
        {
            return { std::move(sig) };
        }

        SharedSignal(SharedSignal const&) = default;
        SharedSignal(SharedSignal&&) noexcept = default;

        SharedSignal& operator=(SharedSignal const&) = default;
        SharedSignal& operator=(SharedSignal&&) noexcept = default;

        SharedSignal clone() const
        {
            return *this;
        }
    };

    template <typename T>
    class SharedSignal<void, T> : public AnySignal<T>
    {
    private:
        SharedSignal(Signal<void, T> sig) :
            AnySignal<T>(std::move(sig))
        {
        }

    public:
        template <typename U>
        SharedSignal(SharedSignal<U, T> sig) :
            AnySignal<T>(std::move(sig))
        {
        }

        template <typename U>
        SharedSignal(Signal<Share<U, T>, T> sig) :
            AnySignal<T>(std::move(sig))
        {
        }

        static SharedSignal create(Signal<void, T>&& sig)
        {
            return { std::move(sig) };
        }

        SharedSignal(SharedSignal const&) = default;
        SharedSignal(SharedSignal&&) noexcept = default;

        SharedSignal& operator=(SharedSignal const&) = default;
        SharedSignal& operator=(SharedSignal&&) noexcept = default;

        SharedSignal clone() const
        {
            return *this;
        }
    };

    template <typename T>
    using AnySharedSignal = SharedSignal<void, T>;
} // namespace reactive::signal


namespace reactive
{
    template <typename T>
    using AnySharedSignal = signal::AnySharedSignal<T>;
} // namespace reactive

