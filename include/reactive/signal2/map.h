#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signalresult.h"
#include "signaltraits.h"

#include "reactive/connection.h"

#include <btl/connection.h>

#include <tuple>

namespace reactive::signal2
{
    template <typename TFunc, typename TSignal>
    class Map
    {
    public:
        struct DataType
        {
            typename TSignal::DataType signalData;
        };

        Map(TFunc func, TSignal sig) :
            func_(std::move(func)),
            sig_(std::move(sig))
        {
        }

        DataType initialize() const
        {
            return { sig_.initialize() };
        }

        auto evaluate(DataType const& data) const
        {
            using ResultType = decltype(std::apply(
                        func_,
                        sig_.evaluate(data.signalData).getTuple()
                        ));

            if constexpr (std::is_same_v<void, ResultType>)
            {
                std::apply(
                        func_,
                        sig_.evaluate(data.signalData).getTuple()
                        );

                return SignalResult<>();
            }
            else if constexpr (IsSignalResult<std::decay_t<ResultType>>::value)
            {
                return std::apply(
                        func_,
                        sig_.evaluate(data.signalData).getTuple()
                        );
            }
            else
            {
                return SignalResult<ResultType>(std::apply(
                            func_,
                            sig_.evaluate(data.signalData).getTuple()
                            ));
            }
        }

        bool hasChanged(DataType const& data) const
        {
            return sig_.hasChanged(data.signalData);
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            return sig_.update(data.signalData, frame);
        }

        template <typename TCallback>
        Connection observe(DataType& data, TCallback&& callback)
        {
            return sig_.observe(data.signalData, std::forward<TCallback>(
                        callback));
        }

    private:
        TFunc func_;
        TSignal sig_;
    };

    template <typename TFunc, typename TSignal>
    struct IsSignal<Map<TFunc, TSignal>> : std::true_type {};
} // namespace reactive::signal

