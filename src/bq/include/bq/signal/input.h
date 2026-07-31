#pragma once

#include "weak.h"
#include "signalresult.h"
#include "datacontext.h"

#include <algorithm>
#include <mutex>
#include <memory>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace bq::signal
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
        std::vector<std::pair<uint64_t, std::weak_ptr<ObserveControl>>> observers;
        uint64_t nextObserverId = 1;
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
            auto control = control_.lock();
            if (!control)
                return;

            std::vector<std::weak_ptr<ObserveControl>> toFire;
            {
                std::unique_lock<std::recursive_mutex> lock(control->mutex);
                control->value = SignalResult<Ts...>(std::forward<Us>(us)...);
                control->valueIndex = std::max(control->valueIndex,
                        control->signalIndex) + 1;
                for (auto it = control->observers.begin();
                        it != control->observers.end(); )
                {
                    if (it->second.expired())
                        it = control->observers.erase(it);
                    else
                    {
                        toFire.push_back(it->second);
                        ++it;
                    }
                }
            }

            for (auto& w : toFire)
                if (auto c = w.lock())
                    c->fire();
        }

        void set(Signal<Weak<Ts...>, std::optional<SignalResult<Ts const&...>>> sig)
        {
            auto control = control_.lock();
            if (!control)
                return;

            std::vector<std::weak_ptr<ObserveControl>> toFire;
            {
                std::unique_lock<std::recursive_mutex> lock(control->mutex);
                control->sig = std::move(sig).unwrap();
                control->signalIndex = std::max(control->valueIndex,
                        control->signalIndex) + 1;
                for (auto it = control->observers.begin();
                        it != control->observers.end(); )
                {
                    if (it->second.expired())
                        it = control->observers.erase(it);
                    else
                    {
                        toFire.push_back(it->second);
                        ++it;
                    }
                }
            }

            for (auto& w : toFire)
                if (auto c = w.lock())
                    c->fire();
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

            ~ContextDataType()
            {
                if (auto control = inputControl.lock())
                {
                    std::unique_lock<std::recursive_mutex> lock(control->mutex);
                    auto& obs = control->observers;
                    for (auto it = obs.begin(); it != obs.end(); ++it)
                        if (it->first == observerId)
                        {
                            obs.erase(it);
                            break;
                        }
                }
            }

            std::optional<SignalDataTypeT<Weak<Ts...>>> sigData;
            SignalResult<Ts...> value;
            std::optional<signal_time_t> updateTime;
            signal_time_t time = signal_time_t(0);
            uint64_t frameId = 0;
            uint64_t index = 0;
            bool didChange = false;
            std::weak_ptr<InputControl<Ts...>> inputControl;
            uint64_t observerId = 0;
        };

        struct DataType
        {
            std::shared_ptr<ContextDataType> contextData;
        };

        InputSignal(std::shared_ptr<InputControl<Ts...>> control) :
            control_(std::move(control))
        {
        }

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            std::unique_lock lock(control_->mutex);

            std::shared_ptr<ContextDataType> contextData =
                context.findData<ContextDataType>(control_->id_);
            if (!contextData)
            {
                contextData = context.initializeData<ContextDataType>(
                        control_->id_, control_->value);

                contextData->index = control_->valueIndex;

                auto id = control_->nextObserverId++;
                control_->observers.emplace_back(id, context.observeControl());
                contextData->inputControl = control_;
                contextData->observerId = id;

                if (control_->sig)
                {
                    contextData->sigData = control_->sig->initialize(context, frame);
                    auto value = control_->sig->evaluate(context,
                            *contextData->sigData);
                    if (value)
                    {
                        contextData->value = std::move(*value);
                        contextData->index = control_->signalIndex;
                    }
                }
            }

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
                contextData.sigData = control_->sig->initialize(context, frame);
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
} // namespace bq::signal

