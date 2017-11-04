#pragma once

#include "signal/share.h"
#include "signal2.h"

namespace reactive::signal
{
    template <typename T, typename TDeferred>
    class Share;
}

namespace reactive::signal2
{
    template <typename T, typename TSignal>
    class SharedSignal : public Signal<T, signal::Share<T, signal::Typed<T, TSignal>>>
    {
    private:
        /*
        SharedSignal(Signal<T, signal::Share<TSignal>> sig) :
            Signal<T, signal::Share<TSignal>>(std::move(sig))
        {
        }
        */

        SharedSignal(signal::Share<T, signal::Typed<T, TSignal>> sig) :
            Signal<T, signal::Share<T, signal::Typed<T, TSignal>>>(std::move(sig))
        {
        }

    public:
        /*
        static SharedSignal create(Signal<T, signal::Share<TSignal>>&& sig)
        {
            return { std::move(sig) };
        }
        */

        static SharedSignal create(signal::Share<T, signal::Typed<T, TSignal>>&& sig)
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
    class SharedSignal<T, void> : public Signal<T, void>
    {
    private:
        SharedSignal(Signal<T, void> sig) :
            Signal<T, void>(std::move(sig))
        {
        }

    public:
        template <typename U>
        SharedSignal(SharedSignal<T, U> sig) :
            Signal<T, void>(std::move(sig))
        {
        }

        static SharedSignal create(Signal<T, void>&& sig)
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

    template <typename T, typename TSignal>
    auto share2(Signal<T, TSignal> sig)
    {
        return signal::share(std::move(sig));
        //return SharedSignal<T, TSignal>::create(std::move(sig));
    }
} // reactive::signal2

