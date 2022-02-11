#pragma once

#include "futurebase.h"
#include "controlwithdata.h"
#include "futureconnection.h"

namespace btl
{
    namespace future
    {
        template <typename T>
        class Future;

        template <typename T>
        class SharedFuture;

        template <typename TFuture>
        auto join(TFuture f)
        -> Future<FutureType<std::decay_t<FutureType<TFuture>>>>
        {
            using T = FutureType<FutureType<TFuture>>;
            using ControlType = detail::ControlWithData<T,
                  FutureConnection>;

            auto control = std::make_shared<ControlType>(f.connect());
            std::weak_ptr<ControlType> weakControl = control;

            std::move(f).listen(
                    [control=std::move(weakControl)](auto&& f2) mutable
                {
                    if (auto p = control.lock())
                    {
                        p->data = f2.connect();
                        std::forward<decltype(f2)>(f2)
                            .listen([control=std::move(control)](auto&& f3)
                            {
                                if (auto p = control.lock())
                                {
                                    p->set(std::forward<decltype(f3)>(f3));
                                }
                            });

                    }
                });


            return { std::move(control) };
        }
    } // future
} // btl

