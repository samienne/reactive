#pragma once

#include "signaltraits.h"
#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"

#include "reactive/connection.h"

#include <iostream>

namespace reactive::signal2
{
    template <typename TStorage>
    class WithChanged
    {
    public:
        using ResultType = ConcatSignalResultsT<SignalResult<bool>,
              SignalTypeT<TStorage>>;

        struct DataType
        {
            DataType(TStorage const& sig) :
                innerData(sig.initialize())
            {
            }

            SignalDataTypeT<TStorage> innerData;
            bool didChange = false;
            bool innerDidChange = false;
        };

        WithChanged(TStorage sig) :
            sig_(std::move(sig))
        {
        }

        DataType initialize() const
        {
            return DataType(sig_);
        }

        ResultType evaluate(DataType const& data) const
        {
            return concatSignalResults(
                    SignalResult<bool>(data.innerDidChange),
                    sig_.evaluate(data.innerData)
                    );
        }

        bool hasChanged(DataType const& data) const
        {
            return data.didChange;
        }

        UpdateResult update(DataType& data, signal2::FrameInfo const& frame)
        {
            auto r = sig_.update(data.innerData, frame);

            data.innerDidChange = r.didChange;
            data.didChange = data.didChange != r.didChange || r.didChange;

            return {
                r.nextUpdate,
                data.didChange
            };
        }

        Connection observe(DataType& data, std::function<void()> callback)
        {
            return sig_.observe(data.innerData, std::move(callback));
        }

    private:
        TStorage sig_;
    };

    template <typename TStorage>
    struct IsSignal<WithChanged<TStorage>> : std::true_type {};
} // reactive::signal2

