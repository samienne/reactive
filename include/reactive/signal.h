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
    template <typename TSignal, typename T>
    class SharedSignal;

    template <typename TSignal, typename T> // defined in typed.h
    class Signal
    {
    public:
        template <typename U, typename V> friend class Signal;

        using NestedSignalType = TSignal;
        using StorageType = std::conditional_t<std::is_same_v<void, TSignal>,
              signal::Share<signal::SignalBase<T>, T>,
              TSignal
            >;

        Signal(StorageType sig) :
            sig_(std::move(sig))
        {
        }

        Signal(SharedSignal<TSignal, T>&& other) noexcept:
            sig_(std::move(other).storage())
        {
        }

        Signal(SharedSignal<TSignal, T>& other) :
            sig_(btl::clone(other.storage()))
        {
        }

        Signal(SharedSignal<TSignal, T> const& other) :
            sig_(btl::clone(other.storage()))
        {
        }

        template <typename USignal, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<USignal, T>>::value
            >>
        Signal(SharedSignal<USignal, T> const& other) :
            sig_(btl::clone(other.storage()))
        {
        }

        template <typename USignal, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<USignal, T>>::value
            >>
        Signal(SharedSignal<USignal, T>&& other) :
            sig_(std::move(other).storage())
        {
        }

        template <typename USignal, typename = std::enable_if_t<
            std::is_base_of<Signal, SharedSignal<USignal, T>>::value
            >>
        Signal(SharedSignal<USignal, T>& other) :
            sig_(other.storage())
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

        StorageType const& storage() const &
        {
            return *sig_;
        }

        StorageType&& storage() &&
        {
            return std::move(*sig_);
        }

        bool isCached() const
        {
            if constexpr(std::is_same_v<void, TSignal>)
            {
                return Signal<void, T>::storage().ptr()->isCached();
            }
            else
            {
                return std::is_reference_v<typename SignalType<StorageType>::type>;
            }
        }

    private:
        template <typename T2> friend class reactive::signal::Weak;
        btl::CloneOnCopy<StorageType> sig_;
    };

    namespace signal
    {
        template <typename TSignal, typename = std::enable_if_t<
            btl::All<
                std::is_convertible<TSignal, std::decay_t<TSignal>>,
                IsSignal<TSignal>
            >::value
            >>
        Signal<std::decay_t<TSignal>, std::decay_t<SignalType<TSignal>>>
        wrap(TSignal&& sig)
        {
            return { std::forward<TSignal>(sig) };
        }

        template <typename T, typename TSignal, typename = std::enable_if_t<
            IsSignal<TSignal>::value
            >>
        auto wrap(Signal<TSignal, T>&& sig)
        {
            return std::move(sig);
        }
    } // namespace signal

    template <typename T>
    class AnySignal : public Signal<void, T>
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
        AnySignal(Signal<TSignal, U>&& other) :
            Signal<void, T>(signal::typed<T>(std::move(other)))
        {
        }

        template <typename USignal>
        AnySignal(SharedSignal<USignal, T> other) :
            Signal<void, T>(std::move(other).storage().ptr())
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
        AnySignal(SharedSignal<USignal, U> other) :
            Signal<void, T>(signal::typed<T>(std::move(other)))
        {
        }

    protected:
        AnySignal(AnySignal const&) = default;
        AnySignal<T>& operator=(AnySignal const&) = default;

    public:
        AnySignal(AnySignal&&) noexcept = default;
        AnySignal<T>& operator=(AnySignal&&) noexcept = default;

        AnySignal clone() const
        {
            return *this;
        }

    private:
        template <typename T2> friend class reactive::signal::Weak;
    };

} // reactive

