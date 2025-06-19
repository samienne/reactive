#pragma once

#include "signaltraits.h"
#include "datacontext.h"

#include <btl/all.h>

namespace reactive::signal
{
    template <typename... Ts>
    class Merge
    {
    public:
        struct DataType
        {
            std::tuple<typename Ts::DataType...> sigData;
        };

        using ResultType = typename ConcatSignalResults<SignalTypeT<Ts>...>::type;

        Merge(std::tuple<Ts...> signals) :
            sigs_(std::move(signals))
        {
        }

        Merge(Merge const&) = default;
        Merge(Merge&&) noexcept = default;

        Merge& operator=(Merge const&) = default;
        Merge& operator=(Merge&&) = default;

        DataType initialize(DataContext& context, FrameInfo const& frame) const
        {
            return doInitialize(context, frame,
                    std::make_index_sequence<sizeof...(Ts)>());
        }

        ResultType evaluate(DataContext& context, DataType const& data) const
        {
            return doEvaluate(context, data, std::make_index_sequence<sizeof...(Ts)>());
        }

        UpdateResult update(DataContext& context, DataType& data, FrameInfo const& frame)
        {
            return doUpdate(context, data, frame,
                    std::make_index_sequence<sizeof...(Ts)>());
        }

        template <typename TCallback>
        Connection observe(DataContext& context, DataType& data, TCallback&& callback)
        {
            return doObserve(context, data, std::forward<TCallback>(callback),
                    std::make_index_sequence<sizeof...(Ts)>());
        }

    private:
        template <size_t... S>
        DataType doInitialize(DataContext& context, FrameInfo const& frame,
                std::index_sequence<S...>) const
        {
            return { std::tuple<typename Ts::DataType...>{
                std::get<S>(sigs_).initialize(context, frame)...
            }};
        }

        template <size_t... S>
        auto doEvaluate(DataContext& context, DataType const& data,
                std::index_sequence<S...>) const
        {
            return makeSignalResultFromTuple(
                    std::tuple_cat(
                        std::get<S>(sigs_).evaluate(
                            context,
                            std::get<S>(data.sigData)
                            ).getTuple()...
                        )
                    );
        }

        template <size_t... S>
        auto doUpdate(DataContext& context, DataType& data, FrameInfo const& frame,
                std::index_sequence<S...>)
        {
            return (UpdateResult {} + ... + std::get<S>(sigs_).update(
                        context, std::get<S>(data.sigData), frame));
        }

        template <typename TCallback, size_t... S>
        auto doObserve(DataContext& context, DataType& data,
                TCallback&& callback, std::index_sequence<S...>)
        {
            return (Connection() + ... + std::get<S>(sigs_).observe(
                        context, std::get<S>(data.sigData), callback));
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
} // namespace reactive::signal

