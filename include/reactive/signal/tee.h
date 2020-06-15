#pragma once

#include "map.h"
#include "inputhandle.h"
#include "cache.h"
#include "droprepeats.h"
#include "removereference.h"
#include "sharedsignal.h"
#include "signaltraits.h"

#include "reactive/connection.h"

namespace reactive::signal
{
    template <typename T1, typename T2>
    class Tee;

    template <typename T1, typename T2>
    struct IsSignal<Tee<T1, T2>> : std::true_type {};

    template <typename T1, typename T2>
    class Tee
    {
    public:
        Tee(AnySharedSignal<T1> upstream, AnySharedSignal<T2> tee) :
            upstream_(upstream),
            tee_(tee)
        {
        }

    private:
        Tee(Tee const&) = default;
        Tee& operator=(Tee const&) = default;

    public:
        Tee(Tee&&) noexcept = default;
        Tee& operator=(Tee&&) noexcept = default;

        auto evaluate() const -> decltype(std::declval<AnySignal<T1>>().evaluate())
        {
            return upstream_.evaluate();
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return upstream_.updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            return upstream_.updateEnd(frame);
        }

        bool hasChanged() const
        {
            return upstream_.hasChanged();
        }

        template <typename TCallback>
        Connection observe(TCallback&& cb)
        {
            return upstream_.observe(std::forward<TCallback>(cb));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("tee()");
            a.addTree(n, upstream_.annotate());
            return a;
        }

        Tee clone() const
        {
            return *this;
        }

    private:
        AnySharedSignal<T1> upstream_;
        AnySharedSignal<T2> tee_;
    };

    static_assert(IsSignal<Tee<int, int>>::value, "Tee is not a signal");

    template <typename TSignal>
    auto tee(TSignal upstream,
            InputHandle<signal_value_t<TSignal>> handle)
    -> AnySignal<signal_value_t<TSignal>>
    {
        auto sig = share(removeReference(
                    tryDropRepeats((std::move(upstream)))
                    ));
        handle.set(weak(sig));
        return sig;
    }

    template <typename T, typename U, typename TMapFunc>
    auto tee(Signal<U, T> sig, TMapFunc&& mapFunc,
            InputHandle<std::decay_t<decltype(
                mapFunc(sig.evaluate()))>> handle)
    /*-> Tee<SignalType<TSignal>,
        std::decay_t<decltype(mapFunc(sig.evaluate()))>
        >*/
    //-> Signal<T>
    {
        AnySharedSignal<T> s1 = share(std::move(sig));

        auto s2 = signal::map(std::forward<TMapFunc>(mapFunc), s1);
        auto teeSig = share(removeReference(signal::tryDropRepeats(
                        std::move(s2))));

        handle.set(weak(teeSig));

        return wrap(Tee<
            T,
            std::decay_t<SignalType<decltype(teeSig)>>
            >(std::move(s1), std::move(teeSig))
            );
    }

} // reactive::signal

