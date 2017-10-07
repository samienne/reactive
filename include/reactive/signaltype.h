#pragma once

#include "signal/frameinfo.h"
#include "signal/signalbase.h"
#include "signaltraits.h"

#include <btl/shared.h>
#include <btl/spinlock.h>
#include <btl/demangle.h>
#include <btl/not.h>
#include <btl/all.h>

#include <mutex>

namespace reactive
{
#if 0
    namespace signal
    {
        template <typename T2> class Weak;
    }

    template <typename T>
    class Signal;


    template <typename T>
    struct IsTypeReduction :
        btl::All<
            IsSignal<T>,
            std::is_same<
                std::decay_t<T>,
                Signal<std::decay_t<SignalType<T>>>
            >
        > {};

    template <typename TSignal, typename T>
    class TypedSignal final : public signal::SignalBase<T>
    {
    public:
        using Lock = std::lock_guard<btl::SpinLock>;

        /*
        TypedSignal(TSignal const& sig) :
            sig_(sig)
        {
        }
        */

        TypedSignal(TSignal&& sig) :
            sig_(std::move(sig))
        {
        }

        ~TypedSignal()
        {
        }

        T evaluate() const override final
        {
            return sig_.evaluate();
        }

        bool hasChanged() const override final
        {
            return sig_.hasChanged();
        }

        btl::option<signal_time_t> updateBegin(signal::FrameInfo const& frame)
            override final
        {
            if (frameId_ == frame.getFrameId())
                return btl::none;

            frameId_ = frame.getFrameId();
            return sig_.updateBegin(frame);
        }

        btl::option<signal_time_t> updateEnd(signal::FrameInfo const& frame)
            override final
        {
            assert(frameId_ == frame.getFrameId());

            if (frameId2_ == frame.getFrameId())
                return btl::none;

            frameId2_ = frame.getFrameId();
            return sig_.updateEnd(frame);
        }


        btl::connection observe(
                std::function<void()> const& callback) override final
        {
            return sig_.observe(callback);
        }

        Annotation annotate() const override final
        {
            //return sig_.annotate();
            return Annotation();
        }

        std::shared_ptr<signal::SignalBase<std::decay_t<T>>>
            cloneDecayed() const override final
        {
            assert(false);
            //return std::make_shared<TypedSignal<TSignal, std::decay_t<T>>>(
                    //sig_.clone());
            return nullptr;
        }

        /*
        std::shared_ptr<signal::SignalBase<std::decay_t<T> const& >>
            cloneConstRef() const override final
        {
            //return std::make_shared<TypedSignal<TSignal, std::decay_t<T> const&>>(
                    //sig_.clone());
            return nullptr;
        }
        */

    private:
        TSignal sig_;
        uint64_t frameId_ = 0;
        uint64_t frameId2_ = 0;
    };

    /*static_assert(IsSignal<TypedSignal<int>>::value,
            "TypedSignal<T> is not a signal");*/

    template <typename T>
    class Signal
    {
    public:
        template <typename U> friend class Signal;

        template <typename TSignal, typename = typename
            std::enable_if
            <
                btl::All<
                    IsSignalType<TSignal, T>,
                    //std::is_convertible<SignalType<TSignal>, T>,
                    btl::Not<IsTypeReduction<TSignal>>
                >::value
            >::type>
        Signal(TSignal signal) :
            deferred_(std::make_shared<TypedSignal<
                    std::decay_t<TSignal>, T>
                    >(
                        std::move(signal)
                        )
                    )
        {
        }

        template <typename T2, typename =
            std::enable_if_t
            <
                std::is_convertible<
                    signal::SignalBase<T2>*,
                    signal::SignalBase<T>*
                    >::value
            >>
        Signal(Signal<T2> const& rhs) :
            deferred_(rhs.deferred_)
        {
        }

        Signal(Signal const&) = default;
        Signal(Signal&&) = default;

        Signal<T>& operator=(Signal const&) = default;
        Signal<T>& operator=(Signal&&) = default;

        template <typename TSignal, typename = typename
            std::enable_if
            <
                IsSignal<TSignal>::value &&
                std::is_convertible<
                    decltype(std::declval<TSignal>().evaluate()),
                    T
                    >::value
            >::type>
        Signal<T>& operator=(TSignal&& signal)
        {
            deferred_ = std::make_shared<
                TypedSignal<std::decay_t<TSignal>, T>
                >(std::forward<TSignal>(signal));

            return *this;
        }



        template <typename T2,
                 typename = std::enable_if_t
                     <
                        std::is_assignable<T, T2>::value
                     >
                >
        Signal<T>& operator=(Signal<T2> const& other)
        {
            deferred_ = other.deferred_;
            return *this;
        }

        T evaluate() const
            /*-> decltype(std::declval<btl::shared<
                    signal::SignalBase<T>>>()->evaluate())*/
        {
            return deferred_->evaluate();
        }


        bool hasChanged() const
        {
            return deferred_->hasChanged();
        }

        signal::UpdateResult updateBegin(signal::FrameInfo const& frame)
        {
            return deferred_->updateBegin(frame);
        }

        signal::UpdateResult updateEnd(signal::FrameInfo const& frame)
        {
            return deferred_->updateEnd(frame);
        }

        template <typename TCallback>
        btl::connection observe(TCallback&& callback)
        {
            return deferred_->observe(std::forward<TCallback>(callback));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("Signal<" + btl::demangle<T>()
                    + "> changed: " + std::to_string(hasChanged()));
            a.addShared(deferred_.raw_ptr(), n, deferred_->annotate());
            return a;
        }

        Signal clone() const
        {
            return *this;
        }

    private:
        template <typename T2> friend class signal::Weak;
        btl::shared<signal::SignalBase<T>> deferred_;
    };

    template <typename TSignal, typename =
             std::enable_if_t
             <
                !IsTypeReduction<TSignal>::value
             >>
    auto makeSignal(TSignal sig) -> Signal<SignalType<TSignal>>
    {
        return Signal<SignalType<TSignal>>(std::move(sig));
    }

    template <typename T>
    auto makeSignal(Signal<T> const& sig) -> Signal<T> const&
    {
        return sig;
    }

    template <typename T>
    auto makeSignal(Signal<T>&& sig) -> Signal<T>
    {
        return std::move(sig);
    }

    template <typename T>
    auto removeReference(Signal<T const&> sig) -> Signal<T>
    {
        return Signal<T>(std::move(sig));
    }

    static_assert(std::is_nothrow_move_constructible<Signal<int>>::value, "");
    static_assert(IsSignal<Signal<int>>::value, "Signal<T> is not a signal");
#endif
} // reactive

