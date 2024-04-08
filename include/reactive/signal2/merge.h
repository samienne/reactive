#pragma once

#include "signaltraits.h"

#include <btl/all.h>

namespace reactive::signal2
{
    template <typename... Ts>
    struct ConcatSignalResults
    {
    };

    template <typename... Ts>
    struct ConcatSignalResults<SignalResult<Ts...>>
    {
        using type = SignalResult<Ts...>;
    };

    template <typename... Ts, typename... Us, typename... Vs>
    struct ConcatSignalResults<SignalResult<Ts...>, SignalResult<Us...>, Vs...>
    {
        using type = typename ConcatSignalResults<
            SignalResult<Ts..., Us...>, Vs...>::type;
    };

    static_assert(std::is_same_v<
            ConcatSignalResults<SignalResult<int, char>, SignalResult<int const&, std::string&>>::type,
            SignalResult<int, char, int const&, std::string&>
            >);

    template <typename... Ts>
    auto concatSignalResults(Ts&&... ts)
    {
        return makeSignalResultFromTuple(std::tuple_cat(std::forward<Ts>(ts)...));
    }
    template <typename... Ts>
    class Merge
    {
    public:
        struct DataType
        {
            std::tuple<typename Ts::DataType...> sigData;
            bool hasChanged = false;
        };

        using ResultType = typename ConcatSignalResults<SignalType<Ts>...>::type;

        Merge(std::tuple<Ts...> signals) :
            sigs_(std::move(signals))
        {
        }

        Merge(Merge const&) = default;
        Merge(Merge&&) noexcept = default;

        Merge& operator=(Merge const&) = default;
        Merge& operator=(Merge&&) = default;

        DataType initialize() const
        {
            return {
                btl::tuple_map(sigs_, [](auto&& sig)
                    {
                        return std::forward<decltype(sig)>(sig).initialize();
                    }),
                    false
            };
        }

        ResultType evaluate(DataType const& data) const
        {
            return doEvaluate(data, std::make_index_sequence<sizeof...(Ts)>());
        }

        bool hasChanged(DataType const& data) const
        {
            return doHasChanged(data, std::make_index_sequence<sizeof...(Ts)>());
        }

        UpdateResult update(DataType& data, FrameInfo const& frame)
        {
            return doUpdate(data, frame, std::make_index_sequence<sizeof...(Ts)>());
        }

        template <typename TCallback>
        Connection observe(DataType& data, TCallback&& callback)
        {
            return doObserve(data, std::forward<TCallback>(callback),
                    std::make_index_sequence<sizeof...(Ts)>());
        }

    private:
        template <size_t... S>
        auto doEvaluate(DataType const& data, std::index_sequence<S...>) const
        {
            return makeSignalResultFromTuple(
                    std::tuple_cat(
                        std::get<S>(sigs_).evaluate(
                            std::get<S>(data.sigData)
                            ).getTuple()...
                        )
                    );
        }

        template <size_t... S>
        bool doHasChanged(DataType const& data, std::index_sequence<S...>) const
        {
            return (false || ... || std::get<S>(sigs_).hasChanged(
                        std::get<S>(data.sigData)));
        }

        template <size_t... S>
        auto doUpdate(DataType& data, FrameInfo const& frame,
                std::index_sequence<S...>)
        {
            return (UpdateResult {} + ... + std::get<S>(sigs_).update(
                    std::get<S>(data.sigData), frame));
        }

        template <typename TCallback, size_t... S>
        auto doObserve(DataType& data, TCallback&& callback, std::index_sequence<S...>)
        {
            return (Connection() + ... + std::get<S>(sigs_).observe(
                        std::get<S>(data.sigData), callback));
        }

    private:
        std::tuple<Ts...> sigs_;
    };

    template <typename... Ts, typename = std::enable_if_t<
        btl::all(IsSignal<std::decay_t<Ts>>::value...)>
        >
    auto merge(Ts&&... signals)
    {
        return wrap(Merge<typename std::decay_t<Ts>::StorageType...>(
                    std::make_tuple(std::forward<Ts>(signals).unwrap()...)
                    ));
    }

    template <typename... Ts>
    struct IsSignal<Merge<Ts...>> : std::true_type {};
} // namespace reactive::signal2

