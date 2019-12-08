#pragma once

#include "signal/share.h"
#include "reactivevisibility.h"

namespace reactive
{
    template <typename TDeferred, typename T>
    class Share;

    template <typename TSignal, typename T>
    class REACTIVE_EXPORT SharedSignal;

    template <typename TSignal, typename T>
    struct IsSignal<SharedSignal<TSignal, T>> : std::true_type {};

    template <typename TSignal, typename T>
    class SharedSignal : public Signal<
                         signal::Share<signal::Typed<TSignal, T>, T>, T
                         >
    {
    private:
        SharedSignal(signal::Share<signal::Typed<TSignal, T>, T> sig) :
            Signal<signal::Share<signal::Typed<TSignal, T>, T>, T>(std::move(sig))
        {
        }

    public:
        static SharedSignal create(signal::Share<signal::Typed<TSignal, T>, T>&& sig)
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

