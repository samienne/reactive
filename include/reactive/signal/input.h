#pragma once

#include "weak.h"
#include "signalresult.h"
#include "datacontext.h"

#include <mutex>
#include <memory>

namespace reactive::signal
{
    template <typename TStorage, typename... Ts>
    class Signal;

    template <typename... Ts>
    struct InputControl
    {
        InputControl(std::tuple<Ts...> value) :
            id_(makeUniqueId()),
            value(std::move(value))
        {
        }

        std::recursive_mutex mutex;
        btl::UniqueId id_;
        SignalResult<Ts...> value;
        std::optional<Weak<Ts...>> sig;
        uint64_t valueIndex = 0;
        uint64_t signalIndex = 0;
    };

    template <typename... Ts>
    class InputHandle
    {
    public:
        InputHandle(std::weak_ptr<InputControl<Ts...>> control) :
            control_(std::move(control))
        {
        }

        InputHandle(InputHandle const&) = default;
        InputHandle(InputHandle&&) noexcept = default;

        InputHandle& operator=(InputHandle const&) = default;
        InputHandle& operator=(InputHandle&&) noexcept = default;

        bool operator==(InputHandle const& rhs) const
        {
            return control_.lock() == rhs.control_.lock();
        }

        bool operator!=(InputHandle const& rhs) const
        {
            return control_.lock() != rhs.control_.lock();
        }

        template <typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Us, Ts>...)
            >>
        void set(Us&&... us)
        {
            if (auto control = control_.lock())
            {
                std::unique_lock lock(control->mutex);
                control->value = SignalResult<Ts...>(std::forward<Us>(us)...);
                control->valueIndex = std::max(control->valueIndex,
                        control->signalIndex) + 1;
            }
        }

        void set(Signal<Weak<Ts...>, std::optional<SignalResult<Ts const&...>>> sig)
        {
            if (auto control = control_.lock())
            {
                std::unique_lock lock(control->mutex);
                control->sig = std::move(sig).unwrap();
                control->signalIndex = std::max(control->valueIndex,
                        control->signalIndex) + 1;
                /*
                control->sigData = control->sig->initialize(control->context);
                control->value = control->sig->evaluate(control->context,
                        *control->sigData);
                ++control->index;
                */
            }
        }

    private:
        std::weak_ptr<InputControl<Ts...>> control_;
    };

    template <typename... Ts>
    class InputSignal
    {
    public:
        struct ContextDataType
        {
            ContextDataType(SignalResult<Ts...> value) :
                value(std::move(value))
            {
            }

            std::optional<SignalDataTypeT<Weak<Ts...>>> sigData;
            SignalResult<Ts...> value;
            std::optional<signal_time_t> updateTime;
            signal_time_t time = signal_time_t(0);
            uint64_t frameId = 0;
            uint64_t index = 0;
            bool didChange = false;
        };

        struct DataType
        {
            std::shared_ptr<ContextDataType> contextData;
        };

        InputSignal(std::shared_ptr<InputControl<Ts...>> control) :
            control_(std::move(control))
        {
        }

        DataType initialize(DataContext& context) const
        {
            std::unique_lock lock(control_->mutex);

            std::shared_ptr<ContextDataType> contextData =
                context.findData<ContextDataType>(control_->id_);
            if (!contextData)
            {
                contextData = context.initializeData<ContextDataType>(
                        control_->id_, control_->value);
            }

            /*
            if (!contextData->index)
                contextData->index = control_->valueIndex;

            if (control_->sig && !contextData->index)
            {
                contextData->index = control_->signalIndex;
                assert(contextData->index);
                contextData->sigData = control_->sig->initialize(context);
                if (auto value = control_->sig->evaluate(context,
                        *contextData->sigData))
                {
                    contextData->value = std::move(*value);
                }
            }
            */

            contextData->index = control_->valueIndex;

            return { contextData };
        }

        SignalResult<Ts const&...> evaluate(DataContext&, DataType const& data) const
        {
            return data.contextData->value;
        }

        UpdateResult update(DataContext& context, DataType& data,
                FrameInfo const& frame)
        {
            std::unique_lock lock(control_->mutex);

            ContextDataType& contextData = *data.contextData;

            bool newFrame = frame.getFrameId() > contextData.frameId;

            if (!newFrame)
            {
                if (contextData.updateTime)
                {
                    return {
                        *contextData.updateTime - contextData.time,
                            contextData.didChange
                    };
                }

                return { std::nullopt, contextData.didChange };
            }

            contextData.frameId = frame.getFrameId();
            contextData.time += frame.getDeltaTime();

            bool didChange = false;
            bool const newSignal = contextData.index < control_->signalIndex;

            if (newSignal && control_->sig)
            {
                contextData.sigData = control_->sig->initialize(context);
                auto value = control_->sig->evaluate(context,
                        *contextData.sigData);

                if (value)
                {
                    contextData.value = std::move(*value);
                    contextData.index = control_->signalIndex;
                    didChange = true;
                }
            }

            if ((newFrame || newSignal) && control_->sig && contextData.sigData)
            {
                auto r = control_->sig->update(context, *contextData.sigData,
                        frame);
                if (r.didChange)
                {
                    contextData.value = *control_->sig->evaluate(
                            context, *contextData.sigData);
                }

                didChange = didChange || r.didChange;

                contextData.updateTime.reset();
                if (r.nextUpdate)
                    contextData.updateTime = contextData.time + *r.nextUpdate;
            }

            bool const newValue = (contextData.index < control_->valueIndex);
            if (newValue)
            {
                contextData.value = control_->value;
                contextData.index = control_->valueIndex;
                didChange = true;
            }

            contextData.didChange = didChange;

            if (contextData.updateTime)
            {
                return {
                    *contextData.updateTime - contextData.time,
                    didChange
                };
            }

            return { std::nullopt, didChange };
        }

        template <typename TCallback>
        Connection observe(DataContext& context, DataType& data, TCallback&& callback)
        {
            std::unique_lock lock(control_->mutex);

            ContextDataType& contextData = *data.contextData;

            if (control_->sig && contextData.sigData)
            {
                return control_->sig->observe(
                        context,
                        *contextData.sigData,
                        std::forward<TCallback>(callback)
                        );
            }

            return {};
        }

    private:
        std::shared_ptr<InputControl<Ts...>> control_;
    };

    template <typename TSignal, typename THandle>
    struct Input
    {
    };

    template <typename... Ts, typename... Us>
    struct Input<SignalResult<Ts...>, SignalResult<Us...>>
    {
        Signal<InputSignal<Ts...>, Ts...> signal;
        InputHandle<Us...> handle;
    };

    template <typename... Ts>
    Input<SignalResult<std::decay_t<Ts>...>, SignalResult<std::decay_t<Ts>...>>
    makeInput(Ts&&... ts)
    {
        auto control = std::make_shared<InputControl<std::decay_t<Ts>...>>(
                std::make_tuple(std::forward<Ts>(ts)...));

        InputHandle<std::decay_t<Ts>...> handle(control);
        auto sig = wrap(InputSignal<std::decay_t<Ts>...>(std::move(control)));

        return {
            std::move(sig),
            std::move(handle)
        };
    }

    template <typename... Ts>
    struct IsSignal<InputSignal<Ts...>> : std::true_type {};
} // namespace reactive::signal

