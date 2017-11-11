#pragma once

#include "signal/share.h"

#include <btl/hidden.h>

namespace reactive
{
    template <typename T, typename TDeferred>
    class Share;

    template <typename T, typename TSignal>
    class SharedSignal : public Signal<T,
    signal::Share<T, signal::Typed<T, TSignal>>>
    {
    private:
        SharedSignal(signal::Share<T, signal::Typed<T, TSignal>> sig) :
            Signal<T, signal::Share<T, signal::Typed<T, TSignal>>>(std::move(sig))
        {
        }

    public:
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
} // reactive

