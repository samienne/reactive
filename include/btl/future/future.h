#pragma once

#include "sharedfuture.h"
#include "promise.h"
#include "futureconnection.h"
#include "futurebase.h"
#include "mbind.h"
#include "fmap.h"
#include "join.h"

#include <btl/moveonlyfunction.h>
#include <btl/foreach.h>
#include <btl/tupleforeach.h>
#include <btl/tuplemap.h>

#include <tuple>
#include <memory>

namespace btl
{
    namespace future
    {
        template <typename T>
        class Future
        {
        public:
            Future(std::shared_ptr<FutureControl<std::decay_t<T>>> base) :
                control_(std::move(base))
            {
            }

            Future(Future const&) = delete;
            Future(Future&&) = default;

            Future& operator=(Future const&) = delete;
            Future& operator=(Future&&) = default;

            template <typename T2 = T,
                typename = std::enable_if_t<!std::is_reference<T2>::value>>
            T get() &&
            {
                return control_->get();
            }

            template <typename T2 = T,
                     typename = std::enable_if_t<std::is_reference<T2>::value>,
                     int = 0>
            T const& get() &&
            {
                return control_->getRef();
            }

            template <typename TFunc, typename... TFutures>
            auto fmap(TFunc&& func, TFutures&&... futures) &&
            -> decltype(
                    future::fmap(
                        std::forward<TFunc>(func),
                        std::move(*this),
                        std::forward<TFutures>(futures)...
                        )
                    )
            {
                return future::fmap(
                        std::forward<TFunc>(func),
                        std::move(*this),
                        std::forward<TFutures>(futures)...
                        );
            }

            template <typename TFunc, typename... TFutures>
            auto mbind(TFunc&& func, TFutures&&... futures) &&
            -> decltype(
                    future::mbind(
                        std::forward<TFunc>(func),
                        std::move(*this),
                        std::forward<TFutures>(futures)...
                        )
                    )
            {
                return future::mbind(
                        std::forward<TFunc>(func),
                        std::move(*this),
                        std::forward<TFutures>(futures)...
                        );
            }

            void listen(btl::MoveOnlyFunction<void(T)> callback) &&
            {
                std::weak_ptr<FutureControl<std::decay_t<T>>> weak(
                        std::move(control_));

                control_->addCallback(
                        [cb=std::move(callback), control=std::move(weak)]()
                        mutable
                    {
                        if (auto p = control.lock())
                        {
                            std::move(cb)(p->get());
                        }
                    });
            }

            FutureConnection connect() const
            {
                return FutureConnection(std::move(control_));
            }

            void addCallback_(std::function<void()> callback)
            {
                control_->addCallback(std::move(callback));
            }

            bool ready() const
            {
                return control_->ready();
            }

            SharedFuture<std::decay_t<T> const&> share() &&
            {
                return { std::move(control_) };
            }

            template <typename T2 = T,
                typename = std::enable_if_t<!std::is_reference<T2>::value>>
            operator Future<std::decay_t<T2> const&>() &&
            {
                return { std::move(control_) };
            }

            template <typename T2 = T,
                typename = std::enable_if_t<std::is_reference<T2>::value>>
            operator Future<std::decay_t<T2>>() &&
            {
                if (control_.unique())
                    return { std::move(control_) };

                using ControlType =
                        detail::ControlWithData
                        <
                            std::decay_t<T>,
                            std::shared_ptr<FutureControl<std::decay_t<T>>>
                        >;

                auto control = std::make_shared<ControlType>(
                        std::move(control_));

                std::weak_ptr<ControlType> weakControl = control;

                control->data->addCallback([control=weakControl]() mutable
                    {
                        if (auto p = control.lock())
                        {
                            p->set(p->data->get());
                        }
                    });

                return { std::move(control) };
            }

            void detach() &&
            {
                auto control = control_;
                control_->addCallback([control=std::move(control)]() mutable
                    {
                    });
            }

        private:
            std::shared_ptr<FutureControl<std::decay_t<T>>> control_;
        };

        template <typename T>
        auto makeReadyFuture(T&& t) -> Future<std::decay_t<T>>
        {
            auto control = std::make_shared<FutureControl<std::decay_t<T>>>();
            control->set(std::forward<T>(t));
            return Future<std::decay_t<T>>(std::move(control));
        }
    } // future
} // btl

