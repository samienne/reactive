#pragma once

#include "signal/share.h"
#include "signal2.h"

namespace reactive::signal
{
    template <typename TDeferred>
    class Share;
}

namespace reactive::signal2
{
    template <typename T, typename TSignal>
    class SharedSignal : public Signal<T, signal::Share<TSignal>>
    {
    private:
        /*
        SharedSignal(Signal<T, signal::Share<TSignal>> sig) :
            Signal<T, signal::Share<TSignal>>(std::move(sig))
        {
        }
        */

        SharedSignal(signal::Share<signal::Typed<TSignal, T>> sig) :
            Signal<T, signal::Share<signal::Typed<TSignal, T>>>(std::move(sig))
        {
        }

    public:
        /*
        static SharedSignal create(Signal<T, signal::Share<TSignal>>&& sig)
        {
            return { std::move(sig) };
        }
        */

        static SharedSignal create(signal::Share<signal::Typed<TSignal, T>>&& sig)
        {
            return { std::move(sig) };
        }

        SharedSignal(SharedSignal const&) = default;
        SharedSignal(SharedSignal&&) = default;

        SharedSignal& operator=(SharedSignal const&) = default;
        SharedSignal& operator=(SharedSignal&&) = default;

        SharedSignal clone() const
        {
            return *this;
        }
    };

    template <typename T>
    class SharedSignal<T, void> : public Signal<T, void>
    {
    public:
        SharedSignal(Signal<T, void> sig) :
            Signal<T, void>(std::move(sig))
        {
        }

        template <typename U>
        SharedSignal(SharedSignal<T, U> sig) :
            Signal<T, void>(std::move(sig))
        {
        }

    public:
        static SharedSignal create(Signal<T, void>&& sig)
        {
            return { std::move(sig) };
        }

        SharedSignal(SharedSignal const&) = default;
        SharedSignal(SharedSignal&&) = default;

        SharedSignal& operator=(SharedSignal const&) = default;
        SharedSignal& operator=(SharedSignal&&) = default;

        SharedSignal clone() const
        {
            return *this;
        }
    };

    template <typename T, typename TSignal>
    auto share2(Signal<T, TSignal> sig)
    {
        return signal::share(std::move(sig));
        //return SharedSignal<T, TSignal>::create(std::move(sig));
    }
} // reactive::signal2

