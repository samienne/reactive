#pragma once

#include "updateresult.h"
#include "frameinfo.h"
#include "signaltraits.h"
#include "datacontext.h"

#include <btl/connection.h>

namespace bq::signal
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

        DataType initialize(DataContext&, FrameInfo const&) const
        {
            return { ResultType{ func_() } };
        }

        ResultType evaluate(DataContext&, DataType const& data) const
        {
            return data.result;
        }

        UpdateResult update(DataContext&, DataType&, FrameInfo const&)
        {
            return {};
        }

        btl::connection observe(DataContext&, DataType&, std::function<void()>)
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
} // bq::signal

