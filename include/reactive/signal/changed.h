#pragma once

#include "constant.h"
#include "reactive/signaltraits.h"

namespace reactive
{
    namespace signal
    {
        template <typename TSignal, typename = typename
            std::enable_if
            <
                IsSignal<TSignal>::value
            >::type>
        class Changed
        {
        public:
            Changed(Changed const&) = default;
            Changed& operator=(Changed const&) = default;

            Changed(TSignal sig):
                signal_(std::move(sig))
            {
            }

            bool evaluate() const
            {
                return changed_ != changedPrevious_;
            }

            bool hasChanged() const
            {
                return changed_ != changedPrevious_;
            }

            UpdateResult updateBegin(FrameInfo const& frame)
            {
                changedPrevious_ = changed_;
                return signal_.updateBegin(frame);
            }

            UpdateResult updateEnd(FrameInfo const& frame)
            {
                auto r = signal_.updateEnd(frame);

                changed_ = signal_.hasChanged();
                if (changed_)
                    return btl::just(signal_time_t(0));

                return r;
            }

            template <typename TFunc>
            Connection observe(TFunc&& f)
            {
                return signal_.observe(std::forward<TFunc>(f));
            }

            Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("changed()");
                a.addTree(n, signal_.annotate());
                return a;
            }

            Changed clone() const
            {
                return *this;
            }

        private:
            Changed(Changed&&) = default;
            Changed& operator=(Changed&&) = default;

        private:
            TSignal signal_;
            bool changedPrevious_ = false;
            bool changed_ = false;
        };

        static_assert(IsSignal<Changed<Constant<int>>>::value,
                "Changed is not a signal");

        template <typename TSignal, typename = typename
            std::enable_if<
                IsSignal<TSignal>::value
            >::type>
        auto changed(TSignal&& sig)
            -> Changed<typename std::decay<TSignal>::type>
        {
            return Changed<typename std::decay<TSignal>::type>(
                    std::forward<TSignal>(sig));
        }
    }
}

