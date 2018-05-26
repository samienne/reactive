#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class BTL_CLASS_VISIBLE Changed;
    }

    template <typename TSignal>
    struct IsSignal<signal::Changed<TSignal>> : std::true_type {};
}

namespace reactive::signal
{
    template <typename TSignal>
    class BTL_CLASS_VISIBLE Changed
    {
    public:
        BTL_HIDDEN Changed(Changed&&) noexcept = default;
        BTL_HIDDEN Changed& operator=(Changed&&) noexcept = default;

        BTL_HIDDEN Changed(TSignal sig):
            signal_(std::move(sig))
        {
        }

        BTL_HIDDEN bool evaluate() const
        {
            return changed_ != changedPrevious_;
        }

        BTL_HIDDEN bool hasChanged() const
        {
            return changed_ != changedPrevious_;
        }

        BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
        {
            changedPrevious_ = changed_;
            return signal_.updateBegin(frame);
        }

        BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
        {
            auto r = signal_.updateEnd(frame);

            changed_ = signal_.hasChanged();
            if (changed_)
                return btl::just(signal_time_t(0));

            return r;
        }

        template <typename TFunc>
        BTL_HIDDEN Connection observe(TFunc&& f)
        {
            return signal_.observe(std::forward<TFunc>(f));
        }

        BTL_HIDDEN Annotation annotate() const
        {
            Annotation a;
            auto&& n = a.addNode("changed()");
            a.addTree(n, signal_.annotate());
            return a;
        }

        BTL_HIDDEN Changed clone() const
        {
            return *this;
        }

    private:
        BTL_HIDDEN Changed(Changed const&) = default;
        BTL_HIDDEN Changed& operator=(Changed const&) = default;

    private:
        TSignal signal_;
        bool changedPrevious_ = false;
        bool changed_ = false;
    };

    template <typename T, typename TSignal>
    auto changed(Signal<T, TSignal> sig)
        -> Changed<Signal<T, std::decay_t<TSignal>>>
    {
        return Changed<Signal<T, std::decay_t<TSignal>>>(std::move(sig));
    }
} // namespace reactive::signal

BTL_VISIBILITY_POP

