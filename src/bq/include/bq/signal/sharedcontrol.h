#pragma once

#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "datacontext.h"

#include <btl/connection.h>

#include <cstdint>
#include <mutex>

namespace bq::signal
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
        virtual std::shared_ptr<BaseDataType> baseInitialize(
                DataContext& context, FrameInfo const& frame) = 0;
        virtual SignalResult<Ts const&...> baseEvaluate(DataContext& context,
                BaseDataType const& data) = 0;
        virtual UpdateResult baseUpdate(DataContext& context, BaseDataType& data,
                FrameInfo const& frame) = 0;
        virtual btl::connection baseObserve(DataContext& context, BaseDataType& data,
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
            ContextDataType(DataContext& context, StorageType const& sig,
                    FrameInfo const& frame) :
                innerData(sig.initialize(context, frame)),
                currentValue(sig.evaluate(context, innerData))
            {
            }

            //ContextDataType(ContextDataType const&) = default;
            //ContextDataType(ContextDataType&&) noexcept = default;

            mutable std::recursive_mutex mutex_;
            typename StorageType::DataType innerData;
            SignalResult<Ts...> currentValue;
            uint64_t lastFrame = 0;
            UpdateResult updateResult;
        };

        struct DataType : Super::BaseDataType
        {
            DataType(SignalResult<Ts...> value,
                    std::shared_ptr<ContextDataType> contextData) :
                value(std::move(value)),
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

            void storeData(DataContext& context)
            {
                if (contextData)
                    context.storeFrameData(contextData);
                else if (auto cd = contextDataWeak.lock())
                    context.storeFrameData(cd);
            }

            SignalResult<Ts...> value;

        private:
            std::shared_ptr<ContextDataType> contextData;
            std::weak_ptr<ContextDataType> contextDataWeak;
        };

        std::shared_ptr<typename Super::BaseDataType> baseInitialize(
                DataContext& context, FrameInfo const& frame) override
        {
            auto data = std::make_shared<DataType>(initialize(context, frame));
            data->storeData(context);
            data->makeWeak();
            return data;
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
            static_cast<DataType&>(data).storeData(context);
            return update(context, static_cast<DataType&>(data), frame);
        }

        btl::connection baseObserve(DataContext& context,
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

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            std::shared_ptr<ContextDataType> contextData =
                context.findData<ContextDataType>(id_);
            if (!contextData)
            {
                contextData = context.initializeData<ContextDataType>(id_,
                        context, sig_, frame);
            }

            DataType data
            {
                contextData->currentValue,
                contextData,
            };

            const_cast<SharedControl&>(*this).update(context, data, frame);

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
            ContextDataType* contextData = data.lock();
            if (!contextData)
            {
                std::cout << "SharedControl: No context data" << std::endl;
                assert(contextData);
                return {};
            }

            std::unique_lock lock(contextData->mutex_);

            if (contextData->lastFrame < frame.getFrameId())
            {
                contextData->lastFrame = frame.getFrameId();
                contextData->updateResult= sig_.update(context,
                        contextData->innerData, frame);

                if (contextData->updateResult.didChange)
                    contextData->currentValue = sig_.evaluate(context,
                            contextData->innerData);
            }

            if (contextData->updateResult.didChange)
                data.value = contextData->currentValue;

            return contextData->updateResult;
        }

        template <typename TCallback>
        btl::connection observe(DataContext& context, DataType& data, TCallback&& callback)
        {
            if (ContextDataType* contextData = data.lock())
            {
                std::unique_lock lock(contextData->mutex_);
                return sig_.observe(context, contextData->innerData, callback);
            }

            return {};
        }

    private:
        btl::UniqueId id_;
        StorageType sig_;
    };
} // namespcae bq::signal

