#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signaltraits.h"
#include "datacontext.h"

#include <btl/connection.h>

namespace bq::signal
{
    template <typename TSignal>
    class Check
    {
    public:
        struct DataType
        {
            DataType(DataContext& context, TSignal const& sig,
                    FrameInfo const& frame) :
                innerData(sig.initialize(context, frame)),
                value(sig.evaluate(context, innerData))
            {
            }

            SignalDataTypeT<TSignal> innerData;
            DecaySignalResultT<SignalTypeT<TSignal>> value;
        };

        Check(TSignal sig) :
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

            auto didReallyChange = false;
            if (r.didChange)
            {
                auto value = sig_.evaluate(context, data.innerData);
                didReallyChange = !(value == data.value);

                if (didReallyChange)
                    data.value = std::move(value);
            }

            return { r.nextUpdate, didReallyChange };
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
    struct IsSignal<Check<T>> : std::true_type {};
} // namespace bq::signal

