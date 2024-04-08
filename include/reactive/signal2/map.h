#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signalresult.h"
#include "signaltraits.h"

#include "reactive/connection.h"

#include <btl/connection.h>

#include <tuple>
#include <type_traits>

namespace reactive::signal2
{
    template <typename TFunc, typename TSignal>
    class Map
    {
    public:
        using InnerResultType = std::optional<std::decay_t<SignalTypeT<TSignal>>>;

        struct DataType
        {
            typename TSignal::DataType signalData;

            // Storage used to temporarily store the signal result before
            // passing it to the map function. This will keep references
            // to temporaries alive as long as DataType is kept in the context
            // and no new calls to evaluate is made.
            mutable InnerResultType innerResult;
        };

        Map(TFunc func, TSignal sig) :
            func_(std::move(func)),
            sig_(std::move(sig))
        {
        }

        DataType initialize() const
        {
            return { sig_.initialize(), std::nullopt };
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
                // Circumvent need for operator= by using copy/move constructor
                data.innerResult.reset();
                new(&data.innerResult) InnerResultType(sig_.evaluate(data.signalData));

                return std::apply(
                        func_,
                        std::move(*data.innerResult).getTuple()
                        );
            }
            else
            {
                // Circumvent need for operator= by using copy/move constructor
                data.innerResult.reset();
                new(&data.innerResult) InnerResultType(sig_.evaluate(data.signalData));

                return SignalResult<ResultType>(std::apply(
                            func_,
                            std::move(*data.innerResult).getTuple()
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

