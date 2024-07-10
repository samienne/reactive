#pragma once

#include "sharedcontrol.h"
#include "signaltraits.h"
#include "datacontext.h"

namespace reactive::signal
{
    /*
    struct DataTypeBase
    {
        virtual ~DataTypeBase() = default;
    };

    template <typename... Ts>
    class WeakControlBase
    {
    public:
        struct DataType
        {
            std::unique_ptr<DataTypeBase> data;
        };

        virtual std::unique_ptr<DataTypeBase> initialize(DataContext& context) const = 0;
        virtual SignalResult<Ts const&...> evaluate(DataContext& context,
                DataTypeBase const& data) const = 0;
        virtual UpdateResult update(DataContext& context, DataTypeBase& data,
                FrameInfo const& frame) = 0;
        virtual btl::connection observe(DataContext& context, DataTypeBase&,
                std::function<void()> callback) = 0;
    };

    template <typename TSharedControl, typename... Ts>
    class WeakControl : public WeakControlBase<Ts...>
    {
    public:
        using InnerData = typename TSharedControl::DataType;
        struct DataType : DataTypeBase
        {
            DataType(SignalResult<Ts...> value) :
                value(std::move(value))
            {
            }

            SignalResult<Ts...> value;
        };

        WeakControl(std::shared_ptr<TSharedControl> control) :
            control_(control),
            innerData_(control->initialize(context_)),
            currentValue_(control->evaluate(context_, innerData_))
        {
        }

        virtual ~WeakControl() = default;

        std::unique_ptr<DataTypeBase> initialize(DataContext&) const override
        {
            std::unique_lock lock(mutex_);

            return std::make_unique<DataType>(currentValue_);
        }

        SignalResult<Ts const&...> evaluate(DataContext&,
                DataTypeBase const& data) const override
        {
            return getData(data).value;
        }

        UpdateResult update(DataContext& context, DataTypeBase& baseData,
                FrameInfo const& frame) override
        {
            std::unique_lock lock(mutex_);

            auto control = control_.lock();

            if (!control)
                return {};

            auto& data = getData(baseData);

            auto r = control->update(context, innerData_, frame);

            if (r.didChange)
                data.value = control->evaluate(context, innerData_);

            return r;
        }

        btl::connection observe(DataContext& context, DataTypeBase&,
                std::function<void()> callback) override
        {
            std::unique_lock lock(mutex_);
            if (auto control = control_.lock())
                return control->observe(context, innerData_, callback);

            return {};
        }

    private:
        static DataType const& getData(DataTypeBase const& data)
        {
            return static_cast<DataType const&>(data);
        }

        static DataType& getData(DataTypeBase& data)
        {
            return static_cast<DataType&>(data);
        }

    private:
        mutable std::mutex mutex_;
        //DataContext context_;
        std::weak_ptr<TSharedControl> control_;
        typename TSharedControl::DataType innerData_;
        SignalResult<Ts...> currentValue_;
    };

    template <typename... Ts>
    class Weak
    {
    public:
        using Control = WeakControlBase<Ts...>;
        using DataType = typename Control::DataType;

        Weak(btl::shared<WeakControlBase<Ts...>> impl) :
            impl_(std::move(impl.ptr()))
        {
        }

        Weak(Weak const&) = default;
        Weak(Weak&&) noexcept = default;

        Weak& operator=(Weak const&) = default;
        Weak& operator=(Weak&&) noexcept = default;

        DataType initialize(DataContext& context) const
        {
            return { impl_->initialize(context) };
        }

        SignalResult<Ts const&...> evaluate(DataContext& context,
                DataType const& data) const
        {
            return impl_->evaluate(context, *data.data);
        }

        UpdateResult update(DataContext& context, DataType& data, FrameInfo const& frame)
        {
            return impl_->update(context, *data.data, frame);
        }

        template <typename TCallback>
        btl::connection observe(DataContext& context, DataType& data,
                TCallback&& callback)
        {
            return impl_->observe(context, *data.data, callback);
        }

    private:
        btl::shared<Control> impl_;
    };
    */

    template <typename... Ts>
    class Weak
    {
    public:
        struct DataType;

        struct ContextDataType
        {
            ContextDataType(DataContext& context,
                    SharedControlBase<Ts...>& control) :
                innerData(control.baseInitialize(context)),
                value(control.baseEvaluate(context, *innerData))
            {
            }

            std::shared_ptr<typename SharedControlBase<Ts...>::BaseDataType> innerData;
            SignalResult<Ts...> value;
            uint64_t frameId = 0;
            bool didChange = false;
            std::optional<signal_time_t> nextUpdate;
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

        DataType initialize(DataContext& context) const
        {
            if (auto control = control_.lock())
            {
                std::shared_ptr<ContextDataType> contextData =
                    context.findData<ContextDataType>(id_);
                if (!contextData)
                {
                    contextData = context.initializeData<ContextDataType>(
                            id_, context, *control);
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
                    data.contextData->nextUpdate = result.nextUpdate;
                    data.contextData->didChange = result.didChange;
                    return result;
                }
                else
                {
                    data.contextData->nextUpdate = std::nullopt;
                    data.contextData->didChange = false;
                }

                data.contextData->frameId = frame.getFrameId();
            }

            return { data.contextData->nextUpdate, data.contextData->didChange };
        }

        Connection observe(DataContext& context, DataType& data,
                std::function<void()> callback)
        {
            if (auto control = control_.lock())
                return control->baseObserve(context,
                        *data.contextData->innerData, std::move(callback));

            return {};
        }

    private:
        btl::UniqueId id_;
        std::weak_ptr<SharedControlBase<Ts...>> control_;
    };

    template <typename... Ts>
    struct IsSignal<Weak<Ts...>> : std::true_type {};
} // namespace reactive::signal

