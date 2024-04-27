#pragma once

#include "updateresult.h"
#include "signalresult.h"
#include "frameinfo.h"
#include "signaltraits.h"

#include "reactive/connection.h"

namespace reactive::signal2
{
    template <typename T>
    struct SignalResultWrapEachToOptional
    {
    };

    template <typename... Ts>
    struct SignalResultWrapEachToOptional<SignalResult<Ts...>>
    {
        using type = SignalResult<std::optional<Ts>...>;
    };

    template <typename T>
    using SignalResultWrapEachToOptionalT =
    typename SignalResultWrapEachToOptional<T>::type;

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

    template <typename T>
    struct MakeNullOptSignalResult
    {
    };

    template <typename... Ts>
    struct MakeNullOptSignalResult<SignalResult<Ts...>>
    {
        static auto make()
        {
            return make(std::make_index_sequence<sizeof...(Ts)>());
        }

        template <size_t... S>
        static auto make(std::index_sequence<S...>)
        {
            return SignalResult<Ts...>(decltype(
                        std::declval<SignalResult<Ts...>>().template get<S>()
                        )(std::nullopt)...
                    );
        }
    };

    template <typename T>
    auto makeEmptySignalResult()
    {
        return MakeNullOptSignalResult<T>::make();
    }

    template <typename TStorage>
    class WithPrevious
    {
    public:
        using InnerResultType = DecaySignalResultT<SignalTypeT<TStorage>>;

        using PreviousResultType =
            SignalResultWrapEachToOptionalT<InnerResultType>;

        using ResultType = ConcatSignalResultsT<
            SignalResultToConstRefT<PreviousResultType>,
            SignalResultToConstRefT<SignalTypeT<TStorage>>
            >;

        struct DataType
        {
            DataType(TStorage const& sig) :
                innerData(sig.initialize()),
                previousResult(makeEmptySignalResult<PreviousResultType>()),
                currentResult(sig.evaluate(innerData))
            {
            }

            SignalDataTypeT<TStorage> innerData;
            PreviousResultType previousResult;
            InnerResultType currentResult;
            bool hasPrevious = false;
            bool didChange = false;
        };

        WithPrevious(TStorage sig) :
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
                    SignalResultToConstRefT<PreviousResultType>(data.previousResult),
                    SignalResultToConstRefT<SignalTypeT<TStorage>>(data.currentResult)
                    );
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
                data.previousResult = std::move(data.currentResult);
                data.currentResult = sig_.evaluate(data.innerData);
                data.hasPrevious = true;
            }
            else
            {
                data.previousResult = makeEmptySignalResult<PreviousResultType>();
                if (data.hasPrevious)
                    r.didChange = true;
                data.hasPrevious = false;
            }

            data.didChange = r.didChange;

            return r;
        }

        Connection observe(DataType& data, std::function<void()> callback)
        {
            return sig_.observe(data.innerData, std::move(callback));
        }

    private:
        TStorage sig_;
    };

    template <typename T>
    struct IsSignal<WithPrevious<T>> : std::true_type {};
} // namespace reactive::signal2

