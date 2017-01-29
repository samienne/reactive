#pragma once

#include "frameinfo.h"
#include "updateresult.h"

#include <reactive/signaltraits.h>
#include <reactive/annotation.h>
#include <reactive/connection.h>

#include <btl/cloneoncopy.h>
#include <btl/demangle.h>

#include <chrono>
#include <utility>

namespace reactive
{
    namespace signal
    {
        enum class TweenType
        {
            linear,
            loop,
            pingpong
        };

        template <typename TSignalValue>
        class Tween
        {
        public:
            Tween(std::chrono::microseconds time, float initial,
                    TSignalValue signal, TweenType type) :
                signal_(std::move(signal)),
                tweenTime_(time),
                currentTime_((uint64_t)(tweenTime_.count() * initial)),
                value_(initial),
                type_(type)
            {
            }

        private:
            Tween(Tween const&) = default;
            Tween& operator=(Tween const&) = default;

        public:
            Tween(Tween&&) = default;
            Tween& operator=(Tween&&) = default;

            Tween clone() const
            {
                return *this;
            }

            float evaluate() const
            {
                return value_;
            }

            bool hasChanged() const
            {
                return changed_;
            }

            UpdateResult updateBegin(signal::FrameInfo const& frame)
            {
                return signal_->updateBegin(frame);
            }

            UpdateResult updateEnd(signal::FrameInfo const& frame)
            {
                auto r = signal_->updateEnd(frame);
                bool changed = signal_->hasChanged();

                if (changed)
                {
                    changed_ = true;
                    up_ = signal_->evaluate();
                }

                if (finished_ && changed)
                {
                    finished_ = false;
                    return btl::just(signal_time_t(0));
                }
                else if (!finished_)
                {
                    if (up_)
                        currentTime_ += frame.getDeltaTime();
                    else
                        currentTime_ -= frame.getDeltaTime();

                    if (currentTime_ > tweenTime_)
                    {
                        if (type_ == TweenType::linear)
                        {
                            currentTime_ = tweenTime_;
                            finished_ = true;
                        }
                        else if (type_ == TweenType::loop)
                            currentTime_ -= tweenTime_;
                        else if (type_ == TweenType::pingpong)
                        {
                            currentTime_ = tweenTime_;
                            up_ = false;
                        }
                    }
                    else if (currentTime_ < std::chrono::microseconds(0))
                    {
                        currentTime_ = std::chrono::microseconds(0);

                        if (type_ == TweenType::pingpong)
                        {
                            up_ = signal_->evaluate();
                            if (!up_)
                                finished_ = true;
                        }
                        else
                        {
                            finished_ = true;
                        }
                    }

                    changed_ = true;

                    value_ = (float)currentTime_.count()
                        / (float)tweenTime_.count();

                    return btl::just(signal_time_t(0));
                }

                changed_ = false;

                return r;
            }

            template <typename TCallback>
            Connection observe(TCallback&& callback)
            {
                return signal_->observe(std::forward<TCallback>(callback));
            }

            Annotation annotate() const
            {
                Annotation a;
                a.addNode("tween<" + btl::demangle<TSignalValue>() + ">");
                return a;
            }

        private:
            btl::CloneOnCopy<TSignalValue> signal_;
            std::chrono::microseconds tweenTime_;
            std::chrono::microseconds currentTime_;
            float value_;
            TweenType type_;
            bool changed_ = false;
            bool finished_ = true;
            bool up_ = false;
        };

        template <typename TSignalValue, typename =
            std::enable_if_t<
                btl::All<
                    IsSignalType<TSignalValue, bool>
                >::value
            >>
        auto tween(std::chrono::microseconds time, float initial,
                TSignalValue value, TweenType type = TweenType::linear)
        {
            return Tween<
                std::decay_t<TSignalValue>
                    >(
                    time,
                    initial,
                    std::move(value),
                    type
                    );
        }
    } // signal
} // reactive

