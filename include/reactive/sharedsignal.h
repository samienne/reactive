#pragma once

#include "signal2.h"

namespace reactive::signal2
{
    template <typename T, typename TSignal = void>
    class SharedSignal : public Signal<T, TSignal>
    {
    private:
        SharedSignal(Signal<T, TSignal>&& sig) :
            Signal<T, TSignal>(std::move(sig))
        {
        }

    public:
        static SharedSignal create(Signal<T, TSignal>&& sig)
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
        return SharedSignal<T, TSignal>::create(std::move(sig));
    }
} // reactive::signal2

