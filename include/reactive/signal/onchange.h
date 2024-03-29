#pragma once

#include "signal.h"
#include "signaltraits.h"

#include <btl/function.h>

#include <type_traits>

namespace reactive::signal
{
    template <typename TSignal, typename TFunc>
    class OnChange;

    template <typename TSignal, typename TFunc>
    struct IsSignal<OnChange<TSignal, TFunc>> : std::true_type {};

    template <typename TSignal, typename TFunc>
    class OnChange
    {
    public:
        using ReturnType = decltype(std::declval<TSignal>().evaluate());

        OnChange(TSignal sig, TFunc func) :
            sig_(std::move(sig)),
            func_(std::move(func))
        {
        }

    private:
        OnChange(OnChange const&) = default;
        OnChange& operator=(OnChange const&) = default;

    public:
        OnChange(OnChange&&) = default;
        OnChange& operator=(OnChange&&) = default;

        ReturnType evaluate() const
        {
            return sig_.evaluate();
        }

        bool hasChanged() const
        {
            return sig_.hasChanged();
        }

        std::optional<signal_time_t> updateBegin(FrameInfo const& frame)
        {
            return sig_.updateBegin(frame);
        }

        std::optional<signal_time_t> updateEnd(FrameInfo const& frame)
        {
            auto r = sig_.updateEnd(frame);

            if (sig_.hasChanged())
                func_(sig_.evaluate());

            return r;
        }

        template <typename TCallback>
        Connection observe(TCallback&& cb)
        {
            return sig_.observe(std::forward<TCallback>(cb));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("onChange()");
            a.addTree(n, sig_.annotate());
            return a;
        }

        OnChange clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<std::decay_t<TSignal>> sig_;
        std::decay_t<TFunc> func_;
    };

    template <typename TSignal, typename TFunc, typename = typename
        std::enable_if
        <
            IsSignal<TSignal>::value
        >::type>
    auto onChange(TSignal&& sig, TFunc&& f)
    -> OnChange<std::decay_t<TSignal>, std::decay_t<TFunc>>
    {
        return OnChange<std::decay_t<TSignal>, std::decay_t<TFunc>>(
                std::forward<TSignal>(sig),
                std::forward<TFunc>(f));
    }
} // namespace reactive::signal

