#pragma once

#include "signal/frameinfo.h"
//#include "signal/share.h"
#include "signal/typed.h"
#include "signal/signalbase.h"
#include "signaltraits.h"

#include <btl/cloneoncopy.h>
#include <btl/shared.h>
#include <btl/spinlock.h>
#include <btl/demangle.h>
#include <btl/not.h>
#include <btl/all.h>
#include <btl/any.h>

#include <mutex>
#include <utility>

namespace reactive::signal
{
    template <typename T2> class Weak;

    template <typename T, typename TDeferred>
    class Share
    {
    public:
        /*
        Share(TSignal sig) :
            control_(std::make_shared<Control>(cache(std::move(sig))))
        {
        }
        */
        Share(std::shared_ptr<TDeferred> deferred) :
            control_(std::move(deferred))
        {
        }

        Share(Share const&) = default;
        Share& operator=(Share const&) = default;
        Share(Share&&) = default;
        Share& operator=(Share&&) = default;

        T evaluate() const// -> decltype(std::declval<TDeferred>().evaluate())
        {
            return control_->evaluate();
        }

        bool hasChanged() const
        {
            return control_->hasChanged();
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return control_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            return control_->updateEnd(frame);
        }

        template <typename TFunc>
        Connection observe(TFunc&& callback)
        {
            return control_->observe(std::forward<TFunc>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            return a;
        }

        Share clone() const
        {
            return *this;
        }

        using ValueType = decltype(std::declval<TDeferred>().evaluate());

        /*
        auto cloneDecayed() const
        {
            return cloneDecayedWithType(*control_);
        }
        */

        std::weak_ptr<SignalBase<ValueType>> weak() const
        {
            return control_.ptr();
        }

        std::shared_ptr<SignalBase<ValueType>> ptr() const &
        {
            return control_.ptr();
        }

        std::shared_ptr<SignalBase<ValueType>> ptr() &&
        {
            return std::move(control_.ptr());
        }

    private:
        /*
        template <typename V, typename U>
        static auto cloneDecayedWithType(signal::Typed<V, U> const& sig)
        {
            return std::make_shared<signal::Typed<std::decay_t<V>, Share>>(
                    Share<V, signal::Typed<V, U>>(std::move(sig))
                    );
        }

        template <typename U>
        static auto cloneDecayedWithType(signal::SignalBase<U> const& sig)
        {
            return Share<std::decay_t<ValueType>, SignalBase<std::decay_t<ValueType>>>(
                    sig.cloneDecayed()
                    );
        }
        */

    private:
        btl::shared<TDeferred> control_;
    };
} // reactive::signal

namespace reactive::signal2
{

    template <typename T, typename TSignal = void>
    class SharedSignal;

    template <typename T, typename TSignal = void>
    class Signal
    {
    public:
        template <typename U, typename V> friend class Signal;

        using NestedSignalType = TSignal;

        Signal(TSignal sig) :
            sig_(std::move(sig))
        {
        }

        Signal(SharedSignal<T, TSignal>&& other) :
            sig_(std::move(other).signal())
        {
        }

        Signal(SharedSignal<T, TSignal>& other) :
            sig_(btl::clone(other.signal()))
        {
        }

        Signal(SharedSignal<T, TSignal> const& other) :
            sig_(btl::clone(other.signal()))
        {
        }

        template <typename USignal, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<T, USignal>>::value
            >>
        Signal(SharedSignal<T, USignal> const& other) :
            sig_(btl::clone(other.signal()))
        {
        }

        template <typename USignal, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<T, USignal>>::value
            >>
        Signal(SharedSignal<T, USignal>&& other) :
            sig_(std::move(other).signal())
        {
        }

        template <typename USignal, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<T, USignal>>::value
            >>
        Signal(SharedSignal<T, USignal>& other) :
            sig_(other.signal())
        {
        }

    protected:
        Signal(Signal const&) = default;
        Signal& operator=(Signal const&) = default;
        Signal& operator=(Signal&&) = default;

    public:
        Signal(Signal&&) = default;

        auto evaluate() const
        {
            return sig_->evaluate();
        }

        bool hasChanged() const
        {
            return sig_->hasChanged();
        }

        signal::UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            return sig_->updateBegin(frame);
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            return sig_->updateEnd(frame);
        }

        template <typename TCallback>
        btl::connection observe(TCallback&& callback)
        {
            return sig_->observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            //auto&& n = a.addNode("Signal<" + btl::demangle<T>()
                    //+ "> changed: " + std::to_string(hasChanged()));
            //a.addShared(sig_.raw_ptr(), n, deferred_->annotate());
            return a;
        }

        Signal clone() const
        {
            return *this;
        }

        TSignal const& signal() const &
        {
            return *sig_;
        }

        TSignal&& signal() &&
        {
            return std::move(*sig_);
        }

    private:
        template <typename T2> friend class reactive::signal::Weak;
        btl::CloneOnCopy<TSignal> sig_;
    };

    template <typename TSignal, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<TSignal, std::decay_t<TSignal>>,
            IsSignal<TSignal>
        >::value
        >>
    Signal<std::decay_t<SignalType<TSignal>>, std::decay_t<TSignal>> wrap(TSignal&& sig)
    {
        return { std::forward<TSignal>(sig) };
    }

    template <typename T, typename TSignal, typename = std::enable_if_t<
        IsSignal<TSignal>::value
        >>
    auto wrap(Signal<T, TSignal>&& sig)
    {
        return std::move(sig);
    }

    template <typename V, typename T, typename U>
    auto typed(Signal<T, U> sig)
    {
        return //signal::Share<V, signal::Typed<V, U>>(
                std::make_shared<signal::Typed<V, U>>(std::move(sig).signal());
                //);
    }

    template <typename U, typename T>
    auto typed(Signal<T, void> sig)
    {
        /*
        return signal::Share<U, signal::Typed<U, signal::SignalBase<T>>>(
                std::move(sig).getDeferredSignalBase().ptr()
                );
                */
        return typed<U>(wrap(signal::Share<T, signal::Typed<T, Signal<T>>>(
                    std::make_shared<signal::Typed<T, Signal<T>>>(std::move(sig)))
                ));
    }

    template <typename T>
    auto typed(Signal<T, void> sig)
    {
        return sig.getDeferredSignalBase();
    }

    template <typename T>
    class Signal<T, void>
    {
    public:
        template <typename U, typename V> friend class Signal;

        using NestedSignalType = void;

        template <typename U, typename TSignal, typename =
            std::enable_if_t<
                btl::All<
                    std::is_same<std::decay_t<T>, std::decay_t<U>>,
                    btl::Any<
                        btl::Not<std::is_reference<T>>,
                        btl::All<std::is_reference<T>, std::is_reference<U>>
                    >
                >::value
            >>
        Signal(Signal<U, TSignal>&& other) :
            /*
            deferred_(std::make_shared<signal::Typed<
                    T, TSignal>>(std::move(other).signal()))
                    */
            deferred_(typed<T>(std::move(other)))
        {
        }

        template <typename USignal>
        Signal(SharedSignal<T, USignal> other) :
            deferred_(std::move(other).signal().ptr())
        {
        }

        /*
        template <typename U, typename = std::enable_if_t<
            btl::All<
                std::is_same<T, U>,
                std::is_same<U, std::decay_t<U>>
                >::value
            >>
        Signal(Signal<T const&, void>&& other) :
            deferred_(other.deferred_.cloneDecayed())
        {
        }
        */

    protected:
        Signal(Signal const&) = default;
        Signal<T>& operator=(Signal const&) = default;

    public:
        Signal(Signal&&) = default;
        Signal<T>& operator=(Signal&&) = default;

        auto evaluate() const
        {
            return deferred_.evaluate();
        }

        bool hasChanged() const
        {
            return deferred_.hasChanged();
        }

        signal::UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            return deferred_.updateBegin(frame);
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            return deferred_.updateEnd(frame);
        }

        template <typename TCallback>
        btl::connection observe(TCallback&& callback)
        {
            return deferred_.observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("Signal<" + btl::demangle<T>()
                    + "> changed: " + std::to_string(hasChanged()));
            //a.addShared(deferred_.raw_ptr(), n, deferred_->annotate());
            return a;
        }

        Signal clone() const
        {
            return *this;
        }

        signal::Share<T, signal::SignalBase<T>> const& signal() const &
        {
            return deferred_;
        }

        signal::Share<T, signal::SignalBase<T>>&& signal() &&
        {
            return std::move(deferred_);
        }

        signal::Share<T, signal::SignalBase<T>> getDeferredSignalBase() &&
        {
            return std::move(deferred_);
        }

    private:
        template <typename T2> friend class reactive::signal::Weak;
        signal::Share<T, signal::SignalBase<T>> deferred_;
    };

    /*
    template <typename T, typename U>
    Signal<std::decay_t<T>, U> removeReference(Signal<T, U> sig)
    {
        return Signal<std::decay_t<T>, U>(std::move(sig));
    }
    */
} // reactive::signal2

namespace reactive::signal
{
    template <typename T, typename U>
    auto makeSignal(signal2::Signal<T, U> sig)
    {
        return signal2::Signal<T, void>(std::move(sig));
    }
} // reactive::signal

