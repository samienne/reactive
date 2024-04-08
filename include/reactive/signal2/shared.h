#pragma once

#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "signaltraits.h"

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

        struct DataType
        {
            SignalResult<Ts...> value;
            uint64_t lastUpdate = 0;
            bool didChange = false;
        };

        SharedImpl(TStorage sig) :
            sig_(std::move(sig)),
            innerData_(sig_.initialize()),
            currentValue_(sig_.evaluate(innerData_))
        {
        }

        DataType initialize() const
        {
            std::unique_lock lock(mutex_);

            DataType data
            {
                currentValue_,
                updateCount_,
            };

            return data;
        }

        bool hasChanged(DataType const& data) const
        {
            return data.didChange;
        }

        SignalResult<Ts const&...> evaluate(DataType const& data) const
        {
            return data.value;
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            std::unique_lock lock(mutex_);

            if (lastFrame_ < frame.getFrameId())
            {
                lastFrame_ = frame.getFrameId();
                UpdateResult innerResult = sig_.update(innerData_, frame);
                if (innerResult.didChange)
                    currentValue_ = sig_.evaluate(innerData_);

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
        btl::connection observe(DataType&, TCallback&& callback)
        {
            std::unique_lock lock(mutex_);
            return sig_.observe(innerData_, callback);
        }

    private:
        mutable std::mutex mutex_;
        TStorage sig_;
        typename TStorage::DataType innerData_;
        SignalResult<Ts...> currentValue_;
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

        Shared(TStorage sig) :
            Shared(std::make_shared<Impl>(std::move(sig)))
        {
        }

        Shared(btl::shared<Impl> impl) :
            impl_(std::move(impl))
        {
        }

        Shared(Shared const&) = default;
        Shared(Shared&&) noexcept = default;

        Shared& operator=(Shared const&) = default;
        Shared& operator=(Shared&&) noexcept = default;

        DataType initialize() const
        {
            return impl_->initialize();
        }

        bool hasChanged(DataType const& data) const
        {
            return impl_->hasChanged(data);
        }

        SignalResult<Ts const&...> evaluate(DataType const& data) const
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

    template <typename... Ts>
    struct IsSignal<Shared<Ts...>> : std::true_type {};
} // namespace reactive::signal2

