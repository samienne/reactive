#pragma once

#include "constant.h"
#include "map.h"
#include "inputhandle.h"
#include "cache.h"
#include "droprepeats.h"

#include "reactive/connection.h"
#include "reactive/signaltraits.h"

#include <fit/identity.h>

namespace reactive
{
    namespace signal
    {

    template <typename T1, typename T2>
    class Tee
    {
    public:
        Tee(Signal<T1> upstream, Signal<T2> tee) :
            upstream_(upstream),
            tee_(tee)
        {
        }

    private:
        Tee(Tee const&) = default;
        Tee& operator=(Tee const&) = default;

    public:
        Tee(Tee&&) = default;
        Tee& operator=(Tee&&) = default;

        auto evaluate() const -> decltype(std::declval<Signal<T1>>().evaluate())
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
        Signal<T1> upstream_;
        Signal<T2> tee_;
    };

    static_assert(IsSignal<Tee<int, int>>::value, "Tee is not a signal");

    template <typename TSignal>
    auto tee(TSignal upstream,
            InputHandle<signal_value_t<TSignal>> handle)
    -> Signal<signal_value_t<TSignal>>
    {
        auto sig = removeReference(
                makeSignal(tryDropRepeats((std::move(upstream))))
                );
        handle.set(sig);
        return std::move(sig);
    }

    template <typename TSignal, typename TMapFunc>
    auto tee(TSignal sig, TMapFunc&& mapFunc,
            InputHandle<std::decay_t<decltype(
                mapFunc(sig.evaluate()))>> handle)
    -> Tee<SignalType<TSignal>,
        std::decay_t<decltype(mapFunc(sig.evaluate()))>
        >
    //-> Signal<T>
    {
        auto s1 = makeSignal(std::move(sig));
        auto teeSig = removeReference(makeSignal(signal::tryDropRepeats(
                    signal::map(std::forward<TMapFunc>(mapFunc), s1))
                ));

        handle.set(teeSig);

        return Tee<
            SignalType<TSignal>,
            std::decay_t<decltype(mapFunc(sig.evaluate()))>
            /*SignalType<
                decltype(mapFunc(teeSig.evaluate()))
                >*/
            >(std::move(s1), std::move(teeSig));
    }

} // signal

} // reactive

