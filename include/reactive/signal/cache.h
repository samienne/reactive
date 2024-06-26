#pragma once

#include "signaltraits.h"
#include "updateresult.h"
#include "frameinfo.h"
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
            DataType(TSignal const& sig) :
                innerData(sig.initialize()),
                value(sig.evaluate(innerData))
            {
            }

            SignalDataTypeT<TSignal> innerData;
            ResultType value;
        };

        Cache(TSignal sig) :
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
            if (r.didChange)
                data.value = sig_.evaluate(data.innerData);

            return r;
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
    struct IsSignal<Cache<T>> : std::true_type {};
} // namespace reactive::signal

