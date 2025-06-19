#pragma once

#include "signaltraits.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "datacontext.h"

#include <btl/connection.h>

namespace bq::signal
{
    template <typename TSignal>
    class Cache
    {
    public:
        using ResultType = SignalTypeT<TSignal>;

        struct DataType
        {
            DataType(DataContext& context, TSignal const& sig,
                    FrameInfo const& frame) :
                innerData(sig.initialize(context, frame)),
                value(sig.evaluate(context, innerData))
            {
            }

            SignalDataTypeT<TSignal> innerData;
            ResultType value;
        };

        Cache(TSignal sig) :
            sig_(std::move(sig))
        {
        }

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            return { context, sig_, frame };
        }

        auto evaluate(DataContext&, DataType const& data) const
        {
            return data.value;
        }

        UpdateResult update(DataContext& context, DataType& data, FrameInfo const& frame)
        {
            auto r = sig_.update(context, data.innerData, frame);
            if (r.didChange)
                data.value = sig_.evaluate(context, data.innerData);

            return r;
        }

        template <typename TCallback>
        btl::connection observe(DataContext& context, DataType& data,
                TCallback&& callback)
        {
            return sig_.observe(context, data.innerData,
                    std::forward<TCallback>(callback));
        }

    private:
        TSignal sig_;
    };

    template <typename T>
    struct IsSignal<Cache<T>> : std::true_type {};
} // namespace bq::signal

