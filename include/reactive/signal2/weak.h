#pragma once

#include "signaltraits.h"

namespace reactive::signal2
{
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

        virtual std::unique_ptr<DataTypeBase> initialize() const = 0;
        virtual bool hasChanged(DataTypeBase const& data) const = 0;
        virtual SignalResult<Ts const&...> evaluate(DataTypeBase const& data) const = 0;
        virtual UpdateResult update(DataTypeBase& data, FrameInfo const& frame) = 0;
        virtual btl::connection observe(DataTypeBase&, std::function<void()> callback) = 0;
    };

    template <typename TSharedControl, typename... Ts>
    class WeakControl : public WeakControlBase<Ts...>
    {
    public:
        using InnerData = typename TSharedControl::DataType;
        struct DataType : DataTypeBase
        {
            DataType(SignalResult<Ts...> value, uint64_t lastUpdate) :
                value(std::move(value)), lastUpdate(lastUpdate)
            {
            }

            SignalResult<Ts...> value;
            uint64_t lastUpdate = 0;
            bool didChange = false;
        };

        WeakControl(std::shared_ptr<TSharedControl> control) :
            control_(control),
            innerData_(control->initialize()),
            currentValue_(control->evaluate(innerData_))
        {
        }

        virtual ~WeakControl() = default;

        std::unique_ptr<DataTypeBase> initialize() const override
        {
            std::unique_lock lock(mutex_);

            return std::make_unique<DataType>(currentValue_, updateCount_);
        }

        bool hasChanged(DataTypeBase const& data) const override
        {
            return getData(data).didChange;
        }

        SignalResult<Ts const&...> evaluate(DataTypeBase const& data) const override
        {
            return getData(data).value;
        }

        UpdateResult update(DataTypeBase& baseData, FrameInfo const& frame) override
        {
            std::unique_lock lock(mutex_);

            auto control = control_.lock();

            if (!control)
                return {};

            auto& data = getData(baseData);

            if (lastFrame_ < frame.getFrameId())
            {
                lastFrame_ = frame.getFrameId();
                UpdateResult innerResult = control->update(innerData_, frame);
                if (innerResult.didChange)
                    currentValue_ = control->evaluate(innerData_);

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

        btl::connection observe(DataTypeBase&,
                std::function<void()> callback) override
        {
            std::unique_lock lock(mutex_);
            if (auto control = control_.lock())
                return control->observe(innerData_, callback);

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
        std::weak_ptr<TSharedControl> control_;
        typename TSharedControl::DataType innerData_;
        SignalResult<Ts...> currentValue_;
        uint64_t lastFrame_ = 0;
        uint64_t updateCount_ = 0;
        signal_time_t time_ = signal_time_t(0);
        std::optional<signal_time_t> updateTime_;
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

        DataType initialize() const
        {
            return { impl_->initialize() };
        }

        bool hasChanged(DataType const& data) const
        {
            return impl_->hasChanged(*data.data);
        }

        SignalResult<Ts const&...> evaluate(DataType const& data) const
        {
            return impl_->evaluate(*data.data);
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            return impl_->update(*data.data, frame);
        }

        template <typename TCallback>
        btl::connection observe(DataType& data, TCallback&& callback)
        {
            return impl_->observe(*data.data, callback);
        }

    private:
        btl::shared<Control> impl_;
    };

    template <typename... Ts>
    struct IsSignal<Weak<Ts...>> : std::true_type {};
} // namespace reactive::signal2

