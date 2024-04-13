#pragma once

#include "signaltraits.h"
#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"

#include "reactive/connection.h"

namespace reactive::signal2
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
            DataType(TCondition const& conditionSignal,
                    AnySignal<Ts...> const& trueSignal,
                    AnySignal<Ts...> const& falseSignal
                    ) :
                conditionData(conditionSignal.initialize()),
                trueData(trueSignal.initialize()),
                falseData(falseSignal.initialize()),
                condition(conditionSignal.evaluate(conditionData).template get<0>())
            {
            }

            SignalDataTypeT<TCondition> conditionData;
            SignalDataTypeT<Signal<void, Ts...>> trueData;
            SignalDataTypeT<Signal<void, Ts...>> falseData;

            bool condition = false;
            bool hasChanged = false;
        };

        Conditional(TCondition conditionSignal, AnySignal<Ts...> trueSignal,
            AnySignal<Ts...> falseSignal) :
            conditionSignal_(std::move(conditionSignal)),
            trueSignal_(std::move(trueSignal)),
            falseSignal_(std::move(falseSignal))
        {
        }

        DataType initialize() const
        {
            return { conditionSignal_, trueSignal_, falseSignal_ };
        }

        SignalResult<Ts...> evaluate(DataType const& data) const
        {
            return data.condition
                ? trueSignal_.evaluate(data.trueData)
                : falseSignal_.evaluate(data.falseData)
                ;
        }

        bool hasChanged(DataType const& data) const
        {
            return data.hasChanged;
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            UpdateResult r = conditionSignal_.update(data.conditionData, frame);
            data.condition = conditionSignal_.evaluate(data.conditionData)
                .template get<0>();

            if (data.condition)
                r = r + trueSignal_.update(data.trueData, frame);
            else
                r = r + falseSignal_.update(data.falseData, frame);

            data.hasChanged = r.didChange;

            return r;
        }

        Connection observe(DataType& data, std::function<void()> const& callback)
        {
            auto c = conditionSignal_.observe(data.conditionData, callback);
            if (data.condition)
                c += trueSignal_.observe(data.trueData, callback);
            else
                c += falseSignal_.observe(data.falseData, callback);

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
} // namespace reactive::signal2
