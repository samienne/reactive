#pragma once

#include "signaltraits.h"
#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "datacontext.h"

#include "reactive/connection.h"

namespace reactive::signal
{
    template <typename TStorage, typename... Ts>
    class Signal;

    template <typename... Ts>
    class AnySignal;

    template <typename TCondition, typename... Ts>
    class Conditional
    {
    public:
        struct DataType
        {
            DataType(DataContext& context,
                    TCondition const& conditionSignal,
                    AnySignal<Ts...> const& trueSignal,
                    AnySignal<Ts...> const& falseSignal,
                    FrameInfo const& frame
                    ) :
                conditionData(conditionSignal.initialize(context, frame)),
                condition(conditionSignal.evaluate(context, conditionData)
                        .template get<0>()),
                signalData(condition
                        ? trueSignal.unwrap().initialize(context, frame)
                        : falseSignal.unwrap().initialize(context, frame))
            {
            }

            SignalDataTypeT<TCondition> conditionData;
            bool condition = false;
            SignalDataTypeT<Signal<void, Ts...>> signalData;

        };

        Conditional(TCondition conditionSignal, AnySignal<Ts...> trueSignal,
            AnySignal<Ts...> falseSignal) :
            conditionSignal_(std::move(conditionSignal)),
            trueSignal_(std::move(trueSignal)),
            falseSignal_(std::move(falseSignal))
        {
        }

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            return { context, conditionSignal_, trueSignal_, falseSignal_, frame };
        }

        SignalResult<Ts...> evaluate(DataContext& context,
                DataType const& data) const
        {
            return data.condition
                ? trueSignal_.unwrap().evaluate(context, data.signalData)
                : falseSignal_.unwrap().evaluate(context, data.signalData)
                ;
        }

        UpdateResult update(DataContext& context, DataType& data,
                FrameInfo const& frame)
        {
            UpdateResult r = conditionSignal_.update(context,
                    data.conditionData, frame);
            bool condition = conditionSignal_.evaluate(context,
                    data.conditionData).template get<0>();

            if (condition != data.condition)
            {
                data.signalData = condition
                    ? trueSignal_.unwrap().initialize(context, frame)
                    : falseSignal_.unwrap().initialize(context, frame)
                    ;
            }
            else
            {
                if (condition)
                    r = r + trueSignal_.unwrap().update(context,
                            data.signalData, frame);
                else
                    r = r + falseSignal_.unwrap().update(context,
                            data.signalData, frame);
            }

            data.condition = condition;

            return r;
        }

        Connection observe(DataContext& context, DataType& data,
                std::function<void()> const& callback)
        {
            auto c = conditionSignal_.observe(context, data.conditionData,
                    callback);
            if (data.condition)
                c += trueSignal_.unwrap().observe(context, data.signalData,
                        callback);
            else
                c += falseSignal_.unwrap().observe(context, data.signalData,
                        callback);

            return c;
        }

    private:
        TCondition conditionSignal_;
        AnySignal<Ts...> trueSignal_;
        AnySignal<Ts...> falseSignal_;
    };

    template <typename... Ts>
    struct IsSignal<Conditional<Ts...>> : std::true_type {};

    template <typename T, typename U, typename V, typename... Ts>
    auto conditional(Signal<T, bool> condition, Signal<U, Ts...> trueSignal,
            Signal<V, Ts...> falseSignal)
    {
        return wrap(Conditional<T, Ts...>(std::move(condition).unwrap(),
                    std::move(trueSignal),
                    std::move(falseSignal)
                    ));
    }
} // namespace reactive::signal
