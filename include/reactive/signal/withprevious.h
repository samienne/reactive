#pragma once

#include "updateresult.h"
#include "signalresult.h"
#include "frameinfo.h"
#include "signaltraits.h"

#include "reactive/connection.h"

#include <btl/copywrapper.h>

namespace reactive::signal
{
    template <typename T>
    struct SignalResultToConstRef
    {
    };

    template <typename... Ts>
    struct SignalResultToConstRef<SignalResult<Ts...>>
    {
        using type = SignalResult<std::decay_t<Ts> const&...>;
    };

    template <typename T>
    using SignalResultToConstRefT = typename SignalResultToConstRef<T>::type;

    template <typename TStorage, typename TFunc, typename... Ts>
    class WithPrevious
    {
    public:
        using FuncReturnType = SignalResult<Ts...>;

        using InnerResultType = SignalTypeT<TStorage>;

        static FuncReturnType evaluateFunc(TFunc const& func,
                FuncReturnType const& current,
                InnerResultType inner)
        {
            return makeSignalResult(std::apply([&](auto&&... ts)
                {
                    using R = decltype(func(std::forward<decltype(ts)>(ts)...));
                    if constexpr(std::is_same_v<void, R>)
                    {
                        func(std::forward<decltype(ts)>(ts)...);
                    }
                    else
                    {
                        return func(std::forward<decltype(ts)>(ts)...);
                    }
                },
                std::tuple_cat(
                    current.getTuple(),
                    std::move(inner).getTuple()
                )));
        }

        struct DataType
        {
            DataType(TFunc const& func, FuncReturnType const& initial,
                    TStorage const& sig) :
                innerData(sig.initialize()),
                innerResult(sig.evaluate(innerData)),
                currentResult(evaluateFunc(
                            func,
                            initial,
                            innerResult
                            ))
            {
            }

            SignalDataTypeT<TStorage> innerData;
            InnerResultType innerResult;
            FuncReturnType currentResult;
            bool hasPrevious = false;
            bool didChange = false;
        };

        WithPrevious(TFunc func, FuncReturnType initial, TStorage sig) :
            func_(std::move(func)),
            initial_(std::make_shared<FuncReturnType>(std::move(initial))),
            sig_(std::move(sig))
        {
        }

        DataType initialize() const
        {
            return DataType(*func_, *initial_, sig_);
        }

        SignalResultToConstRefT<FuncReturnType> evaluate(
                DataType const& data) const
        {
            return data.currentResult;
        }

        bool hasChanged(DataType const& data) const
        {
            return data.didChange;
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            auto r = sig_.update(data.innerData, frame);

            if (r.didChange)
            {
                data.innerResult = sig_.evaluate(data.innerData);
                data.currentResult = evaluateFunc(*func_,
                        data.currentResult, data.innerResult);
            }

            data.didChange = r.didChange;

            return r;
        }

        Connection observe(DataType& data, std::function<void()> callback)
        {
            return sig_.observe(data.innerData, std::move(callback));
        }

    private:
        btl::CopyWrapper<TFunc> func_;
        std::shared_ptr<FuncReturnType> initial_;
        TStorage sig_;
    };

    template <typename TStorage, typename TFunc, typename... Ts>
    struct IsSignal<WithPrevious<TStorage, TFunc, Ts...>> : std::true_type {};
} // namespace reactive::signal

