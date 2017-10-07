#pragma once

#include "signalbase.h"

namespace reactive::signal
{
    template <typename T, typename TSignal>
    class Typed final : public signal::SignalBase<T>
    {
    public:
        using Lock = std::lock_guard<btl::SpinLock>;

        static_assert(
                !std::is_reference<T>::value
                || ( std::is_reference<T>::value
                    && std::is_reference<SignalType<TSignal>>::value),
                "");

        Typed(TSignal&& sig) :
            sig_(std::move(sig))
        {
        }

        ~Typed()
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

        /*
        std::shared_ptr<signal::SignalBase<std::decay_t<T>>>
            cloneDecayed() const override final
        {
            return std::make_shared<Typed<std::decay_t<T>, TSignal>>(
                    btl::clone(sig_));
        }
        */

        /*
        std::shared_ptr<signal::SignalBase<std::decay_t<T> const& >>
            cloneConstRef() const override final
        {
            return std::make_shared<Typed<std::decay_t<T> const&, TSignal>>(
                    btl::clone(sig_));
        }
        */

    private:
        TSignal sig_;
        uint64_t frameId_ = 0;
        uint64_t frameId2_ = 0;
    };
} // reactive::signal

