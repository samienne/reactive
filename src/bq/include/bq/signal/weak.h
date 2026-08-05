#pragma once

#include "sharedcontrol.h"
#include "signaltraits.h"
#include "datacontext.h"

#include <cstdint>

namespace bq::signal
{
    template <typename... Ts>
    class Weak
    {
    public:
        struct DataType;

        struct ContextDataType
        {
            ContextDataType(DataContext& context,
                    SharedControlBase<Ts...>& control,
                    FrameInfo const& frame) :
                innerData(control.baseInitialize(context, frame)),
                value(control.baseEvaluate(context, *innerData))
            {
            }

            std::shared_ptr<typename SharedControlBase<Ts...>::BaseDataType> innerData;
            SignalResult<Ts...> value;
            uint64_t frameId = 0;
            bool didChange = false;
        };

        struct DataType
        {
            std::shared_ptr<ContextDataType> contextData;
        };

        Weak(std::shared_ptr<SharedControlBase<Ts...>> control) :
            id_(makeUniqueId()),
            control_(std::move(control))
        {
        }

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            if (auto control = control_.lock())
            {
                std::shared_ptr<ContextDataType> contextData =
                    context.findData<ContextDataType>(id_);
                if (!contextData)
                {
                    contextData = context.initializeData<ContextDataType>(
                            id_, context, *control, frame);
                }

                return { std::move(contextData) };
            }

            return {};
        }

        std::optional<SignalResult<Ts const&...>> evaluate(
                DataContext&, DataType const& data) const
        {
            if (data.contextData)
                return data.contextData->value;

            return {};
        }

        UpdateResult update(DataContext& context, DataType& data,
                FrameInfo const& frame)
        {
            if (data.contextData->frameId < frame.getFrameId())
            {
                if (auto control = control_.lock())
                {
                    UpdateResult result = control->baseUpdate(context,
                            *data.contextData->innerData, frame);
                    data.contextData->value = control->baseEvaluate(context,
                            *data.contextData->innerData);
                    data.contextData->didChange = result.didChange;
                    return result;
                }
                else
                {
                    data.contextData->didChange = false;
                }

                data.contextData->frameId = frame.getFrameId();
            }

            return { data.contextData->didChange };
        }

    private:
        btl::UniqueId id_;
        std::weak_ptr<SharedControlBase<Ts...>> control_;
    };

    template <typename... Ts>
    struct IsSignal<Weak<Ts...>> : std::true_type {};
} // namespace bq::signal

