#pragma once

#include "controlwithdata.h"
#include "futurecontrol.h"
#include "futurebase.h"
#include "futureconnection.h"

namespace btl::future
{
    template <typename... Ts>
    class Future;

    template <typename... Ts>
    class SharedFuture;

    template <typename TFuture>
    auto join(TFuture f)
    -> Future<FutureType<std::decay_t<FutureType<TFuture>>>>
    {
        using T = FutureType<FutureType<TFuture>>;
        using ControlType = detail::ControlWithData<
            FutureConnection,
            std::decay_t<T>>;

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
} // namespace btl::future

