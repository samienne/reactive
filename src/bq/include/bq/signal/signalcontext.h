#pragma once

#include "signal.h"
#include "datacontext.h"
#include "signaltraits.h"

#include <tuple>
#include <array>
#include <cstddef>
#include <utility>

namespace bq::signal
{
    /** Drives and evaluates a set of signals.
     *
     * Signals are pure values: code builds and transforms them (constructing,
     * mapping, combining) and normally never touches a SignalContext directly.
     * The context is the one place the signal graph is actually run — it owns
     * the per-signal state, advances it once per frame with `update()`, and
     * hands back the current values via `evaluate<I>()` (each signal addressed
     * by its index). In a framework this evaluation is centralized in a single
     * long-lived context; bqui drives a window this way.
     *
     * Prefer one long-lived context over throwaway ones. The state lives in the
     * context, so evaluating a signal in a fresh short-lived context starts from
     * scratch and won't reflect what a live context has accumulated (stream
     * history in particular is not replayed) — reach for the running context,
     * not a quick one made to peek at a value.
     *
     * Several signals can share one context, so a `.share()` node reachable from
     * more than one of them is evaluated once per update, not once per signal.
     */
    template <typename... TSignals>
    class SignalContext
    {
        template <size_t I>
        using EntryResultT =
            SignalTypeT<std::tuple_element_t<I, std::tuple<TSignals...>>>;

    public:
        SignalContext(TSignals... sigs) :
            signals_(std::move(sigs)...),
            data_(initializeData(std::index_sequence_for<TSignals...>{})),
            results_(evaluateAll(std::index_sequence_for<TSignals...>{}))
        {
            dataContext_.swapFrameData();
        }

        UpdateResult update(FrameInfo const& frame)
        {
            return updateImpl(frame, std::index_sequence_for<TSignals...>{});
        }

        template <size_t I>
        EntryResultT<I> const& evaluate() const
        {
            return std::get<I>(results_);
        }

        template <size_t I>
        bool didChange() const
        {
            return changed_[I];
        }

        bool didAnyChange() const
        {
            for (bool c : changed_)
                if (c)
                    return true;
            return false;
        }

    private:
        template <size_t... Is>
        std::tuple<SignalDataTypeT<TSignals>...> initializeData(
                std::index_sequence<Is...>)
        {
            return std::tuple<SignalDataTypeT<TSignals>...>(
                    std::get<Is>(signals_).unwrap().initialize(
                        dataContext_, FrameInfo(0, {}))...);
        }

        // Evaluate one entry and return its result by owned value (copying any
        // references the signal returns into the cache's SignalTypeT).
        template <size_t I>
        EntryResultT<I> evaluateEntry() const
        {
            return std::get<I>(signals_).unwrap().evaluate(
                    dataContext_, std::get<I>(data_));
        }

        template <size_t... Is>
        std::tuple<EntryResultT<Is>...> evaluateAll(std::index_sequence<Is...>)
        {
            return std::tuple<EntryResultT<Is>...>(evaluateEntry<Is>()...);
        }

        template <size_t I>
        UpdateResult updateEntry(FrameInfo const& frame)
        {
            auto r = std::get<I>(signals_).unwrap().update(
                    dataContext_, std::get<I>(data_), frame);
            changed_[I] = r.didChange;
            if (r.didChange)
                std::get<I>(results_) = evaluateEntry<I>();
            return r;
        }

        template <size_t... Is>
        UpdateResult updateImpl(FrameInfo const& frame, std::index_sequence<Is...>)
        {
            UpdateResult result = (UpdateResult{} + ... + updateEntry<Is>(frame));
            dataContext_.swapFrameData();
            return result;
        }

        // evaluate() must be usable on a const SignalContext, but the signal
        // interface takes DataContext by non-const reference for shared/cached
        // lookups. Evaluation does not mutate signal state, so the context is
        // logically const; mark the DataContext mutable to reflect that.
        mutable DataContext dataContext_;
        std::tuple<TSignals...> signals_;
        std::tuple<SignalDataTypeT<TSignals>...> data_;
        std::tuple<SignalTypeT<TSignals>...> results_;
        std::array<bool, sizeof...(TSignals)> changed_ = {};
    };

    template <typename... TSignals>
    SignalContext<std::decay_t<TSignals>...> makeSignalContext(TSignals... sigs)
    {
        return SignalContext<std::decay_t<TSignals>...>(std::move(sigs)...);
    }
} // namespace bq::signal
