#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signaltraits.h"

#include "reactive/connection.h"

namespace reactive::signal
{
    template <typename TSignal>
    class Check
    {
    public:
        struct DataType
        {
            DataType(TSignal const& sig) :
                innerData(sig.initialize()),
                value(sig.evaluate(innerData))
            {
            }

            SignalDataTypeT<TSignal> innerData;
            DecaySignalResultT<SignalTypeT<TSignal>> value;
        };

        Check(TSignal sig) :
            sig_(std::move(sig))
        {
        }

        DataType initialize() const
        {
            return { sig_ };
        }

        auto evaluate(DataType const& data) const
        {
            return data.value;
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            auto r = sig_.update(data.innerData, frame);

            auto didReallyChange = false;
            if (r.didChange)
            {
                auto value = sig_.evaluate(data.innerData);
                didReallyChange = !(value == data.value);

                if (didReallyChange)
                    data.value = std::move(value);
            }

            return { r.nextUpdate, didReallyChange };
        }

        template <typename TCallback>
        Connection observe(DataType& data, TCallback&& callback)
        {
            return sig_.observe(data.innerData, std::forward<TCallback>(callback));
        }

    private:
        TSignal sig_;
    };

    template <typename T>
    struct IsSignal<Check<T>> : std::true_type {};
} // namespace reactive::signal

