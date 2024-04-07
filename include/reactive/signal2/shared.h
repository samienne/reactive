#pragma once

#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"

#include <btl/future/future.h>

#include <btl/async.h>
#include <btl/connection.h>
#include <btl/shared.h>

namespace reactive::signal2
{
    template <typename TStorage, typename... Ts>
    class SharedImpl
    {
    public:
        using StorageType = TStorage;
        using ValueType = typename StorageType::ValueType;

        struct DataType
        {
            btl::future::SharedFuture<Ts...> value;
            uint64_t lastFrame = 0;
            bool didChange = false;
        };

        SharedImpl(TStorage sig) :
            sig_(std::move(sig)),
            innerData_(sig_.initialize()),
            currentValue_(evaluateInner())
        {
        }

        ~SharedImpl()
        {
            currentValue_.cancel();
            currentValue_.wait();
        }

        SharedImpl(SharedImpl const&) = default;
        SharedImpl(SharedImpl&&) noexcept = default;

        SharedImpl& operator=(SharedImpl const&) = default;
        SharedImpl& operator=(SharedImpl&&) noexcept = default;

        DataType initialize()
        {
            std::unique_lock lock(mutex_);

            DataType data
            {
                currentValue_,
                updateCount_,
            };

            return data;
        }

        bool hasChanged(DataType& data) const
        {
            return data.didChange;
        }

        SignalResult<Ts const&...> evaluate(DataType& data) const
        {
            return makeSignalResultFromTuple(data.value.getTuple());
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            std::unique_lock lock(mutex_);

            if (lastFrame_ < frame.getFrameId())
            {
                lastFrame_ = frame.getFrameId();
                UpdateResult innerResult = sig_.update(innerData_, frame);
                currentValue_ = evaluateInner();

                time_ += frame.getDeltaTime();
                updateTime_.reset();
                if (innerResult.nextUpdate)
                    updateTime_ = time_ + *innerResult.nextUpdate;

                if (innerResult.didChange)
                    ++updateCount_;
            }

            data.didChange = false;
            if (data.lastUpdate < updateCount_)
            {
                data.lastUpdate = updateCount_;
                data.value = currentValue_;
                data.didChange = true;
            }

            if (updateTime_)
                return {
                    *updateTime_ - time_,
                    data.didChange
                };

            return {
                std::nullopt,
                data.didChange
            };
        }

        template <typename TCallback>
        btl::connection observe(DataType& data, TCallback&& callback)
        {
            return sig_.observe(data, callback);
        }

    private:
        btl::future::SharedFuture<Ts...> evaluateInner() const
        {
            return btl::async([this]()
                {
                    return sig_.evaluate(innerData_);
                });
        }

    private:
        mutable std::mutex mutex_;
        btl::shared<TStorage> sig_;
        typename TStorage::DataType innerData_;
        btl::future::SharedFuture<Ts...> currentValue_;
        uint64_t lastFrame_ = 0;
        uint64_t updateCount_ = 0;
        signal_time_t time_ = signal_time_t(0);
        std::optional<signal_time_t> updateTime_;
    };

    template <typename TStorage, typename... Ts>
    class Shared
    {
    public:
        using Impl = SharedImpl<TStorage, Ts...>;

        using DataType = typename Impl::DataType;

        Shared(btl::shared<Impl> impl) :
            impl_(std::move(impl))
        {
        }

        Shared(Shared const&) = default;
        Shared(Shared&&) noexcept = default;

        Shared& operator=(Shared const&) = default;
        Shared& operator=(Shared&&) noexcept = default;

        DataType initialize()
        {
            return impl_->initialize();
        }

        bool hasChanged(DataType& data) const
        {
            return impl_->hasChanged(data);
        }

        SignalResult<Ts const&...> evaluate(DataType& data) const
        {
            return impl_->evaluate(data);
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            return impl_->update(data, frame);
        }

        template <typename TCallback>
        btl::connection observe(DataType& data, TCallback&& callback)
        {
            return impl_->observe(data, callback);
        }

    private:
        btl::shared<Impl> impl_;
    };
} // namespace reactive::signal2

