#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signaltraits.h"

#include "reactive/connection.h"

namespace reactive::signal2
{
    template <typename T>
    struct DecaySignalResult
    {
    };

    template <typename... Ts>
    struct DecaySignalResult<SignalResult<Ts...>>
    {
        using type = SignalResult<std::decay_t<Ts>...>;
    };

    template <typename T>
    using DecaySignalResultT = typename DecaySignalResult<T>::type;

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
            bool hasChanged = false;
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

        bool hasChanged(DataType const& data) const
        {
            return data.hasChanged;
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

            data.hasChanged = didReallyChange;

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
} // namespace reactive::signal2

