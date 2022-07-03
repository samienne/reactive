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

        std::move(f).addCallback_(
                [newControl=std::move(weakControl)](auto& control) mutable
            {
                if (auto p = newControl.lock())
                {
                    auto f2 = std::get<0>(std::move(control.getTupleRef()));
                    p->data = f2.connect();
                    std::forward<decltype(f2)>(f2)
                        .addCallback_([newControl=std::move(newControl)](auto& control)
                        {
                            if (auto p = newControl.lock())
                            {
                                auto f3 = std::get<0>(std::move(control.getTupleRef()));
                                p->setValue(std::forward<decltype(f3)>(f3));
                            }
                        });

                }
            });

        return { std::move(control) };
    }
} // namespace btl::future

