#pragma once

#include "map.h"
#include "constant.h"

#include <reactive/signaltraits.h>

#include <btl/function.h>
#include <btl/hidden.h>

#include <type_traits>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TSignal, typename TFunc>
        class BTL_CLASS_VISIBLE OnChange
        {
        public:
            using ReturnType = decltype(std::declval<TSignal>().evaluate());

            BTL_HIDDEN OnChange(TSignal sig, TFunc func) :
                sig_(std::move(sig)),
                func_(std::move(func))
            {
            }

        private:
            BTL_HIDDEN OnChange(OnChange const&) = default;
            BTL_HIDDEN OnChange& operator=(OnChange const&) = default;

        public:
            BTL_HIDDEN OnChange(OnChange&&) = default;
            BTL_HIDDEN OnChange& operator=(OnChange&&) = default;

            BTL_HIDDEN ReturnType evaluate() const
            {
                return sig_.evaluate();
            }

            BTL_HIDDEN bool hasChanged() const
            {
                return sig_.hasChanged();
            }

            BTL_HIDDEN btl::option<signal_time_t> updateBegin(FrameInfo const& frame)
            {
                return sig_.updateBegin(frame);
            }

            BTL_HIDDEN btl::option<signal_time_t> updateEnd(FrameInfo const& frame)
            {
                auto r = sig_.updateEnd(frame);

                if (sig_.hasChanged())
                    func_(sig_.evaluate());

                return r;
            }

            template <typename TCallback>
            BTL_HIDDEN Connection observe(TCallback&& cb)
            {
                return sig_.observe(std::forward<TCallback>(cb));
            }

            BTL_HIDDEN Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("onChange()");
                a.addTree(n, sig_.annotate());
                return a;
            }

            BTL_HIDDEN OnChange clone() const
            {
                return *this;
            }

        private:
            btl::CloneOnCopy<std::decay_t<TSignal>> sig_;
            std::decay_t<TFunc> func_;
        };

        static_assert(IsSignal<
                OnChange<Constant<int>, btl::Function<void()>>>::value, "");

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
    }
}

BTL_VISIBILITY_POP

