#pragma once

#include "signaltraits.h"

#include <btl/forcenoexcept.h>

namespace reactive::signal
{
    template <typename TSignal>
    class UpdateIfJust;

    template <typename TSignal>
    struct IsSignal<UpdateIfJust<TSignal>> : std::true_type {};

    template <typename TSignal>
    class UpdateIfJust
    {
    public:
        using ValueType = decltype(*std::declval<TSignal>().evaluate());
        using ValueTypeDecayed = btl::decay_t<ValueType>;

        UpdateIfJust(TSignal&& signal, ValueTypeDecayed initial) :
            signal_(std::forward<TSignal>(signal)),
            value_(std::move(initial))
        {
        }

    private:
        UpdateIfJust(UpdateIfJust const&) = default;
        UpdateIfJust& operator=(UpdateIfJust const&) = default;

    public:
        UpdateIfJust(UpdateIfJust&&) noexcept = default;
        UpdateIfJust& operator=(UpdateIfJust&&) noexcept = default;

        ValueTypeDecayed const& evaluate() const
        {
            return *value_;
        }

        bool hasChanged() const
        {
            return changed_;
        }

        UpdateResult updateBegin(FrameInfo const& frame)
        {
            return signal_->updateBegin(frame);
        }

        UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = signal_->updateEnd(frame);

            if (signal_->hasChanged())
            {
                auto v = btl::clone(signal_->evaluate());
                changed_ = false;
                if (v.has_value())
                {
                    value_ = std::move(*v);
                    changed_ = true;
                }
            }

            return r;
        }

        template <typename TCallback>
        Connection observe(TCallback&& cb)
        {
            return signal_->observe(std::forward<TCallback>(cb));
        }

        Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("updateIfJust<"
                    + btl::demangle<TSignal>() + ">");
            a.addTree(n, signal_->annotate());
            return a;
        }

        UpdateIfJust clone() const
        {
            return *this;
        }

    private:
        btl::CloneOnCopy<std::decay_t<TSignal>> signal_;
        btl::ForceNoexcept<ValueTypeDecayed> value_;
        bool changed_ = false;
    };


    template <typename TSignal, typename = typename
        std::enable_if<
            IsSignal<TSignal>::value
        >::type
    >
    auto updateIfJust(TSignal&& signal,
            typename UpdateIfJust<TSignal>::ValueTypeDecayed initial)
        //-> UpdateIfJust<TSignal>
    {
        return wrap(
                UpdateIfJust<TSignal>(std::forward<TSignal>(signal),
                    std::move(initial))
                );
    }
} // namespace reactive::signal

