#pragma once

#include "cache.h"
#include "constant.h"
#include "reactive/signaltraits.h"

#include <btl/shared.h>

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class Share
        {
            struct Data
            {
                Data(Cache<std::decay_t<TSignal>> sig) :
                    signal_(std::move(sig))
                {
                }

                Cache<std::decay_t<TSignal>> signal_;
                uint64_t frameId_ = 0;
                uint64_t frameId2_ = 0;
            };

        public:
            Share(TSignal sig) :
                data_(std::make_shared<Data>(cache(std::move(sig))))
            {
            }

            Share(Share const&) = default;
            Share& operator=(Share const&) = default;
            Share(Share&&) = default;
            Share& operator=(Share&&) = default;

            SignalType<TSignal> const& evaluate() const
            {
                return data_->signal_.evaluate();
            }

            bool hasChanged() const
            {
                return data_->signal_.hasChanged();
            }

            UpdateResult updateBegin(FrameInfo const& frame)
            {
                if (data_->frameId_ == frame.getFrameId())
                    return btl::none;

                data_->frameId_ = frame.getFrameId();

                return data_->signal_.updateBegin(frame);
            }

            UpdateResult updateEnd(FrameInfo const& frame)
            {
                assert(data_->frameId_ == frame.getFrameId());

                if (data_->frameId2_ == frame.getFrameId())
                    return btl::none;

                data_->frameId2_ = frame.getFrameId();

                return data_->signal_.updateEnd(frame);
            }

            template <typename TFunc>
            Connection observe(TFunc&& callback)
            {
                return data_->signal_.observe(std::forward<TFunc>(callback));
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

        private:
            btl::shared<Data> data_;
        };

        static_assert(IsSignal<Share<signal::Constant<int>>>::value, "");

        template <typename TSignal, typename = std::enable_if_t<
            IsSignal<TSignal>::value
            >>
        Share<std::decay_t<TSignal>> share(TSignal signal)
        {
            return { std::move(signal) };
        }

        template <typename T>
        auto share(Share<T> sig) -> Share<T>
        {
            return std::move(sig);
        }

        template <typename T>
        auto cache(Share<T> sig) -> Share<T>
        {
            return std::move(sig);
        }

        template <typename T>
        auto share(Signal<T> sig) -> Signal<T>
        {
            return std::move(sig);
        }
    } // signal
} // reactive
