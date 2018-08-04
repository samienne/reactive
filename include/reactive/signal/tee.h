#pragma once

#include "constant.h"
#include "map.h"
#include "inputhandle.h"
#include "cache.h"
#include "droprepeats.h"
#include "removereference.h"

#include "reactive/sharedsignal.h"
#include "reactive/connection.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

#include <fit/identity.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename T1, typename T2>
        class BTL_CLASS_VISIBLE Tee;
    }

    template <typename T1, typename T2>
    struct IsSignal<signal::Tee<T1, T2>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename T1, typename T2>
    class BTL_CLASS_VISIBLE Tee
    {
    public:
        BTL_HIDDEN Tee(SharedSignal<T1> upstream, SharedSignal<T2> tee) :
            upstream_(upstream),
            tee_(tee)
        {
        }

    private:
        BTL_HIDDEN Tee(Tee const&) = default;
        BTL_HIDDEN Tee& operator=(Tee const&) = default;

    public:
        BTL_HIDDEN Tee(Tee&&) noexcept = default;
        BTL_HIDDEN Tee& operator=(Tee&&) noexcept = default;

        BTL_HIDDEN auto evaluate() const -> decltype(std::declval<Signal<T1>>().evaluate())
        {
            return upstream_.evaluate();
        }

        BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
        {
            return upstream_.updateBegin(frame);
        }

        BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
        {
            return upstream_.updateEnd(frame);
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return upstream_.hasChanged();
        }

        template <typename TCallback>
        BTL_HIDDEN Connection observe(TCallback&& cb)
        {
            return upstream_.observe(std::forward<TCallback>(cb));
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("tee()");
            a.addTree(n, upstream_.annotate());
            return a;
        }

        BTL_HIDDEN Tee clone() const
        {
            return *this;
        }

    private:
        SharedSignal<T1> upstream_;
        SharedSignal<T2> tee_;
    };

    static_assert(IsSignal<Tee<int, int>>::value, "Tee is not a signal");

    template <typename TSignal>
    auto tee(TSignal upstream,
            InputHandle<signal_value_t<TSignal>> handle)
    -> Signal<signal_value_t<TSignal>>
    {
        auto sig = share(removeReference(
                    tryDropRepeats((std::move(upstream)))
                    ));
        handle.set(weak(sig));
        return std::move(sig);
    }

    template <typename T, typename U, typename TMapFunc>
    auto tee(Signal<T, U> sig, TMapFunc&& mapFunc,
            InputHandle<std::decay_t<decltype(
                mapFunc(sig.evaluate()))>> handle)
    /*-> Tee<SignalType<TSignal>,
        std::decay_t<decltype(mapFunc(sig.evaluate()))>
        >*/
    //-> Signal<T>
    {
        SharedSignal<T, void> s1 = signal::share(std::move(sig));

        auto s2 = signal::map(std::forward<TMapFunc>(mapFunc), s1);
        auto teeSig = share(removeReference(signal::tryDropRepeats(
                        std::move(s2))));

        handle.set(weak(teeSig));

        return signal::wrap(Tee<
            T,
            std::decay_t<SignalType<decltype(teeSig)>>
            >(std::move(s1), std::move(teeSig))
            );
    }

} // reactive::signal

BTL_VISIBILITY_POP

