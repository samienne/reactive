#pragma once

#include "constant.h"

#include "reactive/signaltraits.h"

#include <btl/forcenoexcept.h>
#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal>
        class BTL_CLASS_VISIBLE UpdateIfJust
        {
        public:
            using ValueType = decltype(*std::declval<TSignal>().evaluate());
            using ValueTypeDecayed = btl::decay_t<ValueType>;

            BTL_HIDDEN UpdateIfJust(TSignal&& signal, ValueTypeDecayed initial) :
                signal_(std::forward<TSignal>(signal)),
                value_(std::move(initial))
            {
            }

        private:
            BTL_HIDDEN UpdateIfJust(UpdateIfJust const&) = default;
            BTL_HIDDEN UpdateIfJust& operator=(UpdateIfJust const&) = default;

        public:
            BTL_HIDDEN UpdateIfJust(UpdateIfJust&&) noexcept = default;
            BTL_HIDDEN UpdateIfJust& operator=(UpdateIfJust&&) noexcept = default;

            BTL_HIDDEN ValueTypeDecayed const& evaluate() const
            {
                return *value_;
            }

            BTL_HIDDEN bool hasChanged() const
            {
                return changed_;
            }

            BTL_HIDDEN UpdateResult updateBegin(signal::FrameInfo const& frame)
            {
                return signal_->updateBegin(frame);
            }

            BTL_HIDDEN UpdateResult updateEnd(signal::FrameInfo const& frame)
            {
                auto r = signal_->updateEnd(frame);

                if (signal_->hasChanged())
                {
                    auto v = btl::clone(signal_->evaluate());
                    changed_ = false;
                    if (v.valid())
                    {
                        value_ = std::move(*v);
                        changed_ = true;
                    }
                }

                return r;
            }

            template <typename TCallback>
            BTL_HIDDEN Connection observe(TCallback&& cb)
            {
                return signal_->observe(std::forward<TCallback>(cb));
            }

            BTL_HIDDEN Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("updateIfJust<"
                        + btl::demangle<TSignal>() + ">");
                a.addTree(n, signal_->annotate());
                return a;
            }

            BTL_HIDDEN UpdateIfJust clone() const
            {
                return *this;
            }

        private:
            btl::CloneOnCopy<std::decay_t<TSignal>> signal_;
            btl::ForceNoexcept<ValueTypeDecayed> value_;
            bool changed_ = false;
        };

        static_assert(IsSignal<UpdateIfJust<Constant<btl::option<int>>>>::value, "");

        template <typename TSignal, typename = typename
            std::enable_if<
                IsSignal<TSignal>::value
            >::type
        >
        auto updateIfJust(TSignal&& signal,
                typename UpdateIfJust<TSignal>::ValueTypeDecayed initial)
            //-> UpdateIfJust<TSignal>
        {
            return signal::wrap(
                    UpdateIfJust<TSignal>(std::forward<TSignal>(signal),
                        std::move(initial))
                    );
        }
    } // signal
} // reactive

BTL_VISIBILITY_POP

