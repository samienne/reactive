#pragma once

#include "updateresult.h"
#include "frameinfo.h"
#include "signaltraits.h"

#include "reactive/connection.h"

namespace reactive::signal
{
    template <typename TFunc>
    class EvaluateOnInit
    {
    public:
        using ResultType = ToSignalResultT<std::invoke_result_t<TFunc>>;
        struct DataType
        {
            ResultType result;
        };

        EvaluateOnInit(TFunc func) :
            func_(std::move(func))
        {
        }

        DataType initialize() const
        {
            return { ResultType{ func_() } };
        }

        ResultType evaluate(DataType const& data) const
        {
            return data.result;
        }

        bool hasChanged(DataType const&) const
        {
            return false;
        }

        UpdateResult update(DataType&, FrameInfo const&)
        {
            return {};
        }

        Connection observe(DataType&, std::function<void()>)
        {
            return {};
        }

    private:
        TFunc func_;
    };

    template <typename TFunc>
    struct IsSignal<EvaluateOnInit<TFunc>> : std::true_type {};

    template <typename TFunc>
    auto evaluateOnInit(TFunc&& func)
    {
        return wrap(EvaluateOnInit<std::decay_t<TFunc>>(std::forward<
                TFunc>(func)));
    }
} // reactive::signal

