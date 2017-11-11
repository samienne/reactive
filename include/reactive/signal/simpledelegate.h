#pragma once

#include "share.h"
#include "databind.h"

#include "reactive/signal/updateifjust.h"
#include "reactive/signal.h"
#include "reactive/signaltraits.h"

#include <btl/option.h>
#include <btl/hidden.h>

#include <type_traits>
#include <utility>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename TDelegate>
        class BTL_CLASS_VISIBLE SimpleDelegate
        {
        public:
            BTL_HIDDEN SimpleDelegate(TDelegate delegate) :
                delegate_(std::move(delegate))
            {
            }

            template <typename T, typename = std::enable_if_t<
                IsSignal<T>::value
                >
            >
            BTL_HIDDEN auto operator()(T&& t, IndexSignal index)
            /*
                -> Signal<btl::option<std::decay_t<
                    decltype(
                            std::declval<TDelegate>()(
                                std::declval<SharedSignal<
                                    SignalType<T>
                                    >>(),
                                index
                                )
                            )
                    >>>
                    */
            {
                auto tShared = signal::share(std::forward<T>(t));

                auto isJust = map([](signal_value_t<T> const& opt) -> bool
                        {
                            return opt.valid();
                        }, tShared);

                auto value = updateIfJust(
                        tShared,
                        btl::clone(*tShared.evaluate())
                        );

                auto delegated = delegate_(std::move(value), std::move(index));
                auto valueOpt = constant(btl::just(std::move(delegated)));

                return conditional(
                        std::move(isJust),
                        std::move(valueOpt),
                        signal::constant<signal_value_t<decltype(valueOpt)>>(
                            btl::none)
                        );
            }

        private:
            typename std::decay<TDelegate>::type delegate_;
        };

        template <typename TDelegate>
        auto simpleDelegate(TDelegate delegate)
            //-> SimpleDelegate<TDelegate>
        {
            return SimpleDelegate<std::decay_t<TDelegate>>(
                    std::move(delegate)
                    );
        }
    } //signal
} // reactive

BTL_VISIBILITY_POP

