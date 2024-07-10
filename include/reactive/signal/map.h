#pragma once

#include "frameinfo.h"
#include "updateresult.h"
#include "signalresult.h"
#include "signaltraits.h"
#include "datacontext.h"

#include "reactive/connection.h"

#include <btl/copywrapper.h>
#include <btl/connection.h>

#include <tuple>
#include <type_traits>

namespace reactive::signal
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

        DataType initialize(DataContext& context) const
        {
            return { sig_.initialize(context), std::nullopt };
        }

        auto evaluate(DataContext& context, DataType const& data) const
        {
            using ResultType = decltype(std::apply(
                        *func_,
                        sig_.evaluate(context, data.signalData).getTuple()
                        ));

            if constexpr (std::is_same_v<void, ResultType>)
            {
                std::apply(
                        *func_,
                        sig_.evaluate(context, data.signalData).getTuple()
                        );

                return SignalResult<>();
            }
            else if constexpr (IsSignalResult<std::decay_t<ResultType>>::value)
            {
                // Circumvent need for operator= by using copy/move constructor
                data.innerResult.reset();
                new(&data.innerResult) InnerResultType(sig_.evaluate(
                            context, data.signalData));

                return std::apply(
                        *func_,
                        std::move(*data.innerResult).getTuple()
                        );
            }
            else
            {
                // Circumvent need for operator= by using copy/move constructor
                data.innerResult.reset();
                new(&data.innerResult) InnerResultType(sig_.evaluate(
                            context, data.signalData));

                return SignalResult<ResultType>(std::apply(
                            *func_,
                            std::move(*data.innerResult).getTuple()
                            ));
            }
        }

        UpdateResult update(DataContext& context, DataType& data, FrameInfo const& frame)
        {
            return sig_.update(context, data.signalData, frame);
        }

        template <typename TCallback>
        Connection observe(DataContext& context, DataType& data, TCallback&& callback)
        {
            return sig_.observe(context, data.signalData, std::forward<TCallback>(
                        callback));
        }

    private:
        mutable btl::CopyWrapper<TFunc> func_;
        TSignal sig_;
    };

    template <typename TFunc, typename TSignal>
    struct IsSignal<Map<TFunc, TSignal>> : std::true_type {};
} // namespace reactive::signal

