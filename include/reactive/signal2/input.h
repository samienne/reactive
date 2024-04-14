#pragma once

#include "weak.h"
#include "signalresult.h"

#include <mutex>
#include <memory>

namespace reactive::signal2
{
    template <typename TStorage, typename... Ts>
    class Signal;

    template <typename... Ts>
    struct InputControl
    {
        InputControl(std::tuple<Ts...> value) :
            value(std::move(value))
        {
        }

        std::recursive_mutex mutex;
        std::optional<SignalDataTypeT<Weak<Ts...>>> sigData;
        std::optional<Weak<Ts...>> sig;
        std::optional<signal_time_t> updateTime;
        signal_time_t time = signal_time_t(0);
        SignalResult<Ts...> value;
        uint64_t index = 0;
        uint64_t frameId = 0;
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

        template <typename... Us, typename = std::enable_if_t<
            btl::all(std::is_convertible_v<Us, Ts>...)
            >>
        void set(Us&&... us)
        {
            if (auto control = control_.lock())
            {
                std::unique_lock lock(control->mutex);
                control->value = SignalResult<Ts...>(std::forward<Us>(us)...);
                control->sig.reset();
                control->sigData.reset();
                ++control->index;
            }
        }

        void set(Signal<Weak<Ts...>, Ts...> sig)
        {
            if (auto control = control_.lock())
            {
                std::unique_lock lock(control->mutex);
                control->sig = std::move(sig).unwrap();
                control->sigData = control->sig->initialize();
                control->value = control->sig->evaluate(*control->sigData);
                ++control->index;
            }
        }

    private:
        std::weak_ptr<InputControl<Ts...>> control_;
    };

    template <typename... Ts>
    class InputSignal
    {
    public:
        struct DataType
        {
            SignalResult<Ts...> value;
            uint64_t index = 0;
            bool hasChanged = false;
        };

        InputSignal(std::shared_ptr<InputControl<Ts...>> control) :
            control_(std::move(control))
        {
        }

        DataType initialize() const
        {
            std::unique_lock lock(control_->mutex);

            return {
                control_->value,
                control_->index
            };
        }

        SignalResult<Ts const&...> evaluate(DataType const& data) const
        {
            return data.value;
        }

        bool hasChanged(DataType const& data) const
        {
            return data.hasChanged;
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            std::unique_lock lock(control_->mutex);

            bool newFrame = frame.getFrameId() > control_->frameId;
            control_->frameId = frame.getFrameId();

            if (newFrame)
                control_->time += frame.getDeltaTime();

            if (newFrame && control_->sig && control_->sigData)
            {
                auto r = control_->sig->update(*control_->sigData, frame);
                if (r.didChange)
                {
                    control_->value = control_->sig->evaluate(*control_->sigData);
                    ++control_->index;
                }

                control_->updateTime.reset();
                if (r.nextUpdate)
                    control_->updateTime = control_->time + *r.nextUpdate;
            }

            data.hasChanged = (data.index < control_->index);
            if (data.hasChanged)
            {
                data.value = control_->value;
                data.index = control_->index;
            }

            if (control_->updateTime)
            {
                return {
                    *control_->updateTime - control_->time,
                    data.hasChanged
                };
            }

            return { std::nullopt, data.hasChanged };
        }

        template <typename TCallback>
        Connection observe(DataType&, TCallback&& callback)
        {
            std::unique_lock lock(control_->mutex);
            if (control_->sig && control_->sigData)
            {
                return control_->sig->observe(
                        *control_->sigData,
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

