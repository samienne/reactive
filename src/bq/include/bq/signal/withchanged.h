#pragma once

#include "signaltraits.h"
#include "signalresult.h"
#include "updateresult.h"
#include "frameinfo.h"
#include "datacontext.h"

#include <btl/connection.h>

namespace bq::signal
{
    template <typename TStorage>
    class WithChanged
    {
    public:
        using ResultType = ConcatSignalResultsT<SignalResult<bool>,
              SignalTypeT<TStorage>>;

        struct DataType
        {
            DataType(DataContext& context, TStorage const& sig,
                    FrameInfo const& frame) :
                innerData(sig.initialize(context, frame))
            {
            }

            SignalDataTypeT<TStorage> innerData;
            bool innerDidChange = false;
        };

        WithChanged(TStorage sig, bool ignoreChange) :
            sig_(std::move(sig)),
            ignoreChange_(ignoreChange)
        {
        }

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            return DataType(context, sig_, frame);
        }

        ResultType evaluate(DataContext& context, DataType const& data) const
        {
            return concatSignalResults(
                    SignalResult<bool>(data.innerDidChange),
                    sig_.evaluate(context, data.innerData)
                    );
        }

        UpdateResult update(DataContext& context, DataType& data,
                signal::FrameInfo const& frame)
        {
            auto r = sig_.update(context, data.innerData, frame);

            bool changedStatusChanged = ignoreChange_
                ? false
                : data.innerDidChange != r.didChange
                ;

            bool didChange = changedStatusChanged || r.didChange;
            data.innerDidChange = r.didChange;

            return {
                r.nextUpdate,
                didChange
            };
        }

        btl::connection observe(DataContext& context, DataType& data,
                std::function<void()> callback)
        {
            return sig_.observe(context, data.innerData, std::move(callback));
        }

    private:
        TStorage sig_;
        bool ignoreChange_ = false;
    };

    template <typename TStorage>
    struct IsSignal<WithChanged<TStorage>> : std::true_type {};
} // bq::signal

