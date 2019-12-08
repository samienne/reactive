#pragma once

#include "signal/share.h"
#include "reactivevisibility.h"

namespace reactive
{
    template <typename TDeferred, typename T>
    class Share;

    template <typename TStorage, typename T>
    class REACTIVE_EXPORT SharedSignal;

    template <typename TStorage, typename T>
    struct IsSignal<SharedSignal<TStorage, T>> : std::true_type {};

    template <typename TStorage, typename T>
    class SharedSignal : public Signal<
                         signal::Share<signal::Typed<TStorage, T>, T>, T
                         >
    {
    private:
        SharedSignal(signal::Share<signal::Typed<TStorage, T>, T> sig) :
            Signal<signal::Share<signal::Typed<TStorage, T>, T>, T>(std::move(sig))
        {
        }

    public:
        static SharedSignal create(signal::Share<signal::Typed<TStorage, T>, T>&& sig)
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
        SharedSignal(Signal<signal::Share<U, T>, T> sig) :
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
} // namespace reactive

