#pragma once

#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "datacontext.h"

#include "reactive/connection.h"

#include <cstdint>
#include <mutex>


namespace reactive::signal
{
    template <typename... Ts>
    class SharedControlBase
    {
    public:
        struct BaseDataType
        {
            virtual ~BaseDataType() = default;
        };

        virtual ~SharedControlBase() = default;
        virtual std::shared_ptr<BaseDataType> baseInitialize(DataContext& context) = 0;
        virtual SignalResult<Ts const&...> baseEvaluate(DataContext& context,
                BaseDataType const& data) = 0;
        virtual UpdateResult baseUpdate(DataContext& context, BaseDataType& data,
                FrameInfo const& frame) = 0;
        virtual Connection baseObserve(DataContext& context, BaseDataType& data,
                std::function<void()> callback) = 0;
    };

    template <typename TStorage, typename... Ts>
    class SharedControl : public SharedControlBase<Ts...>
    {
    public:
        using Super = SharedControlBase<Ts...>;
        using StorageType = TStorage;

        struct ContextDataType
        {
            ContextDataType(DataContext& context, StorageType const& sig) :
                innerData(sig.initialize(context)),
                currentValue(sig.evaluate(context, innerData))
            {
            }

            ContextDataType(ContextDataType const&) = default;
            ContextDataType(ContextDataType&&) noexcept = default;

            typename StorageType::DataType innerData;
            SignalResult<Ts...> currentValue;
            uint64_t lastFrame = 0;
            uint64_t updateCount = 0;
            signal_time_t time = signal_time_t(0);
            std::optional<signal_time_t> updateTime;
        };

        struct DataType : Super::BaseDataType
        {
            DataType(SignalResult<Ts...> value,
                    uint64_t lastUpdate,
                    std::shared_ptr<ContextDataType> contextData) :
                value(std::move(value)),
                lastUpdate(lastUpdate),
                contextData(std::move(contextData))
            {
            }

            DataType(DataType const&) = default;
            DataType(DataType&&) noexcept = default;

            ContextDataType* lock()
            {
                if (contextData)
                    return contextData.get();

                if (auto cd = contextDataWeak.lock())
                    return cd.get();

                return nullptr;
            }

            DataType& makeWeak()
            {
                if (contextData)
                {
                    contextDataWeak = contextData;
                    contextData.reset();
                }

                return *this;
            }

            SignalResult<Ts...> value;
            uint64_t lastUpdate = 0;

        private:
            std::shared_ptr<ContextDataType> contextData;
            std::weak_ptr<ContextDataType> contextDataWeak;
        };

        std::shared_ptr<typename Super::BaseDataType> baseInitialize(
                DataContext& context) override
        {
            return std::make_shared<DataType>(initialize(context).makeWeak());
        }

        SignalResult<Ts const&...> baseEvaluate(DataContext& context,
                typename Super::BaseDataType const& data) override
        {
            return evaluate(context, static_cast<DataType const&>(data));
        }

        UpdateResult baseUpdate(DataContext& context,
                typename Super::BaseDataType& data,
                FrameInfo const& frame) override
        {
            return update(context, static_cast<DataType&>(data), frame);
        }

        Connection baseObserve(DataContext& context,
                typename Super::BaseDataType& data,
                std::function<void()> callback) override
        {
            return observe(context, static_cast<DataType&>(data),
                    std::move(callback));
        }

        SharedControl(StorageType sig) :
            id_(makeUniqueId()),
            sig_(std::move(sig))
        {
        }

        DataType initialize(DataContext& context) const
        {
            std::unique_lock lock(mutex_);

            std::shared_ptr<ContextDataType> contextData =
                context.findData<ContextDataType>(id_);
            if (!contextData)
            {
                contextData = context.initializeData<ContextDataType>(id_,
                        context, sig_);
            }

            DataType data
            {
                sig_.evaluate(context, contextData->innerData),
                contextData->updateCount,
                std::move(contextData),
            };

            return data;
        }

        SignalResult<Ts const&...> evaluate(DataContext&,
                DataType const& data) const
        {
            return data.value;
        }

        UpdateResult update(DataContext& context, DataType& data,
                FrameInfo const& frame)
        {
            std::unique_lock lock(mutex_);

            ContextDataType* contextData = data.lock();
            if (!contextData)
                return {};

            if (contextData->lastFrame < frame.getFrameId())
            {
                contextData->lastFrame = frame.getFrameId();
                UpdateResult innerResult = sig_.update(context,
                        contextData->innerData, frame);

                if (innerResult.didChange)
                    contextData->currentValue = sig_.evaluate(context,
                            contextData->innerData);

                contextData->time += frame.getDeltaTime();
                contextData->updateTime.reset();
                if (innerResult.nextUpdate)
                    contextData->updateTime = contextData->time
                        + *innerResult.nextUpdate;

                if (innerResult.didChange)
                    ++contextData->updateCount;
            }

            bool didChange = false;
            if (data.lastUpdate < contextData->updateCount)
            {
                data.lastUpdate = contextData->updateCount;
                data.value = contextData->currentValue;
                didChange = true;
            }

            if (contextData->updateTime)
                return {
                    *contextData->updateTime - contextData->time,
                    didChange
                };

            return {
                std::nullopt,
                didChange
            };
        }

        template <typename TCallback>
        Connection observe(DataContext& context, DataType& data, TCallback&& callback)
        {
            std::unique_lock lock(mutex_);
            if (ContextDataType* contextData = data.lock())
                return sig_.observe(context, contextData->innerData, callback);

            return {};
        }

    private:
        mutable std::recursive_mutex mutex_;
        btl::UniqueId id_;
        StorageType sig_;
    };
} // namespcae reactive::signal

