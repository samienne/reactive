#pragma once

#include "signal/frameinfo.h"
#include "signal/typed.h"
#include "signal/signalbase.h"
#include "signaltraits.h"
#include "reactivevisibility.h"

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
} // reactive::signal

namespace reactive
{
    template <typename T, typename TSignal = void>
    class SharedSignal;

    template <typename T, typename TSignal /*= void */> // defined in typed.h
    class Signal
    {
    public:
        template <typename U, typename V> friend class Signal;

        using NestedSignalType = TSignal;

        Signal(TSignal sig) :
            sig_(std::move(sig))
        {
        }

        Signal(SharedSignal<T, TSignal>&& other) noexcept:
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
        Signal& operator=(Signal&&) noexcept = default;

    public:
        Signal(Signal&&) noexcept = default;

        auto evaluate() const -> decltype(auto)
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

    namespace signal
    {
    template <typename TSignal, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<TSignal, std::decay_t<TSignal>>,
            IsSignal<TSignal>
        >::value
        >>
    Signal<std::decay_t<SignalType<TSignal>>, std::decay_t<TSignal>>
    wrap(TSignal&& sig)
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

    } // namespace signal

    template <typename T>
    class Signal<T, void>
    {
    public:
        template <typename U, typename V> friend class Signal;

        using NestedSignalType = void;

        template <typename U, typename TSignal, typename =
            std::enable_if_t<
                btl::All<
                    std::is_convertible<std::decay_t<U>, std::decay_t<T>>,
                    btl::Any<
                        btl::Not<std::is_reference<T>>,
                        btl::All<std::is_reference<T>, std::is_reference<U>>
                    >
                >::value
            >>
        Signal(Signal<U, TSignal>&& other) :
            deferred_(signal::typed<T>(std::move(other)))
        {
        }

        template <typename USignal>
        Signal(SharedSignal<T, USignal> other) :
            deferred_(std::move(other).signal().ptr())
        {
        }

        template <typename U, typename USignal, typename =
            std::enable_if_t<
                btl::All<
                    std::is_convertible<std::decay_t<U>, std::decay_t<T>>,
                    btl::Any<
                        btl::Not<std::is_reference<T>>,
                        btl::All<std::is_reference<T>, std::is_reference<U>>
                    >
                >::value
            >>
        Signal(SharedSignal<U, USignal> other) :
            deferred_(signal::typed<T>(std::move(other)))
        {
        }

    protected:
        Signal(Signal const&) = default;
        Signal<T>& operator=(Signal const&) = default;

    public:
        Signal(Signal&&) noexcept = default;
        Signal<T>& operator=(Signal&&) noexcept = default;

        auto evaluate() const -> decltype(auto)
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
            /*auto&& n = a.addNode("Signal<" + btl::demangle<T>()
                    + "> changed: " + std::to_string(hasChanged()));*/
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

        bool isCached() const
        {
            return deferred_.ptr()->isCached();
        }

    private:
        template <typename T2> friend class reactive::signal::Weak;
        signal::Share<T, signal::SignalBase<T>> deferred_;
    };
} // reactive

