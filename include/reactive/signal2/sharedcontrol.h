#pragma once

#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"

#include "reactive/connection.h"

#include <cstdint>
#include <mutex>


namespace reactive::signal2
{
    template <typename TStorage, typename... Ts>
    class SharedControl
    {
    public:
        using StorageType = TStorage;

        struct DataType
        {
            SignalResult<Ts...> value;
            uint64_t lastUpdate = 0;
            bool didChange = false;
        };

        SharedControl(StorageType sig) :
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
        Connection observe(DataType&, TCallback&& callback)
        {
            std::unique_lock lock(mutex_);
            return sig_.observe(innerData_, callback);
        }

    private:
        mutable std::recursive_mutex mutex_;
        StorageType sig_;
        typename StorageType::DataType innerData_;
        SignalResult<Ts...> currentValue_;
        uint64_t lastFrame_ = 0;
        uint64_t updateCount_ = 0;
        signal_time_t time_ = signal_time_t(0);
        std::optional<signal_time_t> updateTime_;
    };
} // namespcae reactive::signal2

