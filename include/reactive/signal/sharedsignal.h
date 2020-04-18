#pragma once

#include "share.h"

namespace reactive::signal
{
    template <typename TDeferred, typename... Ts>
    class Share;

    template <typename TStorage, typename... Ts>
    class SharedSignal;

    template <typename TStorage, typename... Ts>
    struct IsSignal<SharedSignal<TStorage, Ts...>> : std::true_type {};

    template <typename TStorage, typename... Ts>
    class SharedSignal : public Signal<
                         Share<Typed<TStorage, Ts...>, Ts...>, Ts...
                         >
    {
    private:
        SharedSignal(Share<Typed<TStorage, Ts...>, Ts...> sig) :
            Signal<Share<Typed<TStorage, Ts...>, Ts...>, Ts...>(std::move(sig))
        {
        }

    public:
        static SharedSignal create(Share<Typed<TStorage, Ts...>, Ts...>&& sig)
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

    template <typename... Ts>
    class SharedSignal<void, Ts...> : public AnySignal<Ts...>
    {
    private:
        SharedSignal(Signal<void, Ts...> sig) :
            AnySignal<Ts...>(std::move(sig))
        {
        }

    public:
        template <typename U>
        SharedSignal(SharedSignal<U, Ts...> sig) :
            AnySignal<Ts...>(std::move(sig))
        {
        }

        template <typename U>
        SharedSignal(Signal<Share<U, Ts...>, Ts...> sig) :
            AnySignal<Ts...>(std::move(sig))
        {
        }

        static SharedSignal create(Signal<void, Ts...>&& sig)
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

