#pragma once

#include "signal.h"
#include "signaltraits.h"

#include <btl/tuplemap.h>
#include <btl/reduce.h>
#include <btl/fmap.h>
#include <btl/foreach.h>

#include <vector>

namespace reactive::signal
{
    template <typename TSignals>
    class Combine;

    template <typename TSignals>
    struct IsSignal<Combine<TSignals>> : std::true_type {};

    template <typename TSignals>
    class Combine
    {
    public:
        Combine(TSignals signals) :
            sigs_(std::move(signals))
        {
        }

    private:
        Combine(Combine const&) = default;
        Combine& operator=(Combine const&) = default;

    public:
        Combine(Combine&&) noexcept(true) = default;
        Combine& operator=(Combine&&) noexcept(true) = default;

        auto evaluate() const
        {
            return btl::fmap(*sigs_, [](auto&& sig)
                    {
                        return btl::clone(sig.evaluate());
                    });
        }

        bool hasChanged() const
        {
            return btl::reduce(false, *sigs_,
                    [](bool initial, auto const& sig) noexcept
                    {
                        return initial || sig.hasChanged();
                    });
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return btl::reduce(
                    std::optional<signal_time_t>(std::nullopt), *sigs_,
                    [&frame](std::optional<signal_time_t> t, auto& sig) noexcept
                    {
                        auto t2 = sig.updateBegin(frame);
                        return min(t, t2);
                    });
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            return btl::reduce(
                    std::optional<signal_time_t>(std::nullopt), *sigs_,
                    [&frame](std::optional<signal_time_t> t, auto& sig) noexcept
                    {
                        auto t2 = sig.updateEnd(frame);
                        return min(t, t2);
                    });
        }

        template <typename TCallback>
        Connection observe(TCallback&& callback) noexcept
        {
            return btl::reduce(Connection(), *sigs_,
                    [callback = std::forward<TCallback>(callback)]
                    (Connection c, auto& sig) noexcept
                    {
                        return std::move(c) + sig.observe(callback);
                    });
        }

        Annotation annotate() const noexcept
        {
            Annotation a;
            auto n = a.addNode("combine()");

            btl::forEach(*sigs_, [&a, &n](auto&& sig)
                {
                    a.addTree(n, sig.annotate());
                });

            return a;
        }

        Combine clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<btl::decay_t<TSignals>> sigs_;
    };

    template <typename T, typename = std::enable_if_t<
        IsSignal<T>::value
        >
    >
    auto combine(std::vector<T> signals)
        ///-> Combine<std::vector<T>>
    {
        return wrap(Combine<std::vector<T>>(std::move(signals)));
    }

    template <typename... Ts>
    auto combine(std::tuple<Ts...> signals)
        //-> Combine<std::tuple<Ts...>>
    {
        return wrap(Combine<std::tuple<Ts...>>(std::move(signals)));
    }
} // namespace reactive::signal:

