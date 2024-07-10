#pragma once

#include "signaltraits.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "datacontext.h"

#include "reactive/connection.h"

namespace reactive::signal
{
    template <typename TSignal>
    class Cache
    {
    public:
        using ResultType = SignalTypeT<TSignal>;

        struct DataType
        {
            DataType(DataContext& context, TSignal const& sig) :
                innerData(sig.initialize(context)),
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

        DataType initialize(DataContext& context) const
        {
            return { context, sig_ };
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
        Connection observe(DataContext& context, DataType& data,
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
} // namespace reactive::signal

