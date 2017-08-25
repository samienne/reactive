#pragma once

#include "signal/frameinfo.h"
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

namespace reactive::signal2
{
    template <typename TSignal, typename T>
    class TypedSignal final : public signal::SignalBase<T>
    {
    public:
        using Lock = std::lock_guard<btl::SpinLock>;

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
            return std::make_shared<TypedSignal<TSignal, std::decay_t<T>>>(
                    btl::clone(sig_));
        }

        std::shared_ptr<signal::SignalBase<std::decay_t<T> const& >>
            cloneConstRef() const override final
        {
            return std::make_shared<TypedSignal<TSignal, std::decay_t<T> const&>>(
                    btl::clone(sig_));
        }

    private:
        TSignal sig_;
        uint64_t frameId_ = 0;
        uint64_t frameId2_ = 0;
    };

    template <typename T, typename TSignal>
    class SharedSignal;

    template <typename T, typename TSignal = void>
    class Signal
    {
    public:
        template <typename U, typename V> friend class Signal;

        Signal(TSignal sig) :
            sig_(std::move(sig))
        {
        }

        template <typename USignal>
        Signal(SharedSignal<T, USignal> other) :
            sig_(std::move(other.sig_))
        {
        }

    protected:
        Signal(Signal const&) = default;
        Signal& operator=(Signal const&) = default;
        Signal& operator=(Signal&&) = default;

    public:
        Signal(Signal&&) = default;

        T evaluate() const
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
        btl::CloneOnCopy<TSignal> sig_;
    };

    template <typename TSignal, typename = std::enable_if_t<
        btl::All<
            std::is_convertible<TSignal, std::decay_t<TSignal>>,
            IsSignal<TSignal>
        >::value
        >>
    Signal<SignalType<TSignal>, std::decay_t<TSignal>> wrap(TSignal&& sig)
    {
        return { std::forward<TSignal>(sig) };
    }

    template <typename T, typename TSignal>
    auto wrap(Signal<T, TSignal>&& sig)
    {
        return std::move(sig);
    }

    template <typename T>
    class Signal<T, void>
    {
    public:
        template <typename U, typename V> friend class Signal;

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
            deferred_(std::make_shared<TypedSignal<
                    std::decay_t<TSignal>, T>>(std::move(*other.sig_)))
        {
        }

        template <typename = std::enable_if_t<
                std::is_same<T, std::decay_t<T>>::value
            >>
        Signal(Signal<T const&, void>&& other) :
            deferred_(other.deferred_->cloneDecayed())
        {
        }

    protected:
        Signal(Signal const&) = default;
        Signal<T>& operator=(Signal const&) = default;

    public:
        Signal(Signal&&) = default;
        Signal<T>& operator=(Signal&&) = default;

        T evaluate() const
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

        Signal const& signal() const &
        {
            return *this;
        }

        Signal&& signal() &&
        {
            return std::move(*this);
        }

    private:
        //template <typename T2> friend class signal::Weak;
        btl::shared<signal::SignalBase<T>> deferred_;
    };
}

