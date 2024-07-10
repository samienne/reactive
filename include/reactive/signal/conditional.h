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
                    AnySignal<Ts...> const& falseSignal
                    ) :
                conditionData(conditionSignal.initialize(context)),
                trueData(trueSignal.unwrap().initialize(context)),
                falseData(falseSignal.unwrap().initialize(context)),
                condition(conditionSignal.evaluate(context, conditionData)
                        .template get<0>())
            {
            }

            SignalDataTypeT<TCondition> conditionData;
            SignalDataTypeT<Signal<void, Ts...>> trueData;
            SignalDataTypeT<Signal<void, Ts...>> falseData;

            bool condition = false;
        };

        Conditional(TCondition conditionSignal, AnySignal<Ts...> trueSignal,
            AnySignal<Ts...> falseSignal) :
            conditionSignal_(std::move(conditionSignal)),
            trueSignal_(std::move(trueSignal)),
            falseSignal_(std::move(falseSignal))
        {
        }

        DataType initialize(DataContext& context) const
        {
            return { context, conditionSignal_, trueSignal_, falseSignal_ };
        }

        SignalResult<Ts...> evaluate(DataContext& context,
                DataType const& data) const
        {
            return data.condition
                ? trueSignal_.unwrap().evaluate(context, data.trueData)
                : falseSignal_.unwrap().evaluate(context, data.falseData)
                ;
        }

        UpdateResult update(DataContext& context, DataType& data,
                FrameInfo const& frame)
        {
            UpdateResult r = conditionSignal_.update(context,
                    data.conditionData, frame);
            data.condition = conditionSignal_.evaluate(context,
                    data.conditionData).template get<0>();

            if (data.condition)
                r = r + trueSignal_.unwrap().update(context, data.trueData,
                        frame);
            else
                r = r + falseSignal_.unwrap().update(context, data.falseData,
                        frame);

            return r;
        }

        Connection observe(DataContext& context, DataType& data,
                std::function<void()> const& callback)
        {
            auto c = conditionSignal_.observe(context, data.conditionData,
                    callback);
            if (data.condition)
                c += trueSignal_.unwrap().observe(context, data.trueData,
                        callback);
            else
                c += falseSignal_.unwrap().observe(context, data.falseData,
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
