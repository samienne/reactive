#pragma once

#include "futureconnection.h"
#include "futurebase.h"
#include "mbind.h"
#include "fmap.h"

#include <btl/moveonlyfunction.h>

namespace btl
{
    namespace future
    {
        template <typename T>
        class Future;

        template <typename T>
        class SharedFuture
        {
        public:
            SharedFuture(std::shared_ptr<FutureControl<std::decay_t<T>>> base) :
                control_(std::move(base))
            {
            }

            SharedFuture(SharedFuture const&) = default;
            SharedFuture(SharedFuture&&) = default;

            SharedFuture& operator=(SharedFuture const&) = default;
            SharedFuture& operator=(SharedFuture&&) = default;

            std::decay_t<T> const& get() const
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
            auto fmap(TFunc&& func, TFutures&&... futures) const &
            -> decltype(
                    future::fmap(
                        std::forward<TFunc>(func),
                        *this,
                        std::forward<TFutures>(futures)...
                        )
                    )
            {
                return future::fmap(
                        std::forward<TFunc>(func),
                        *this,
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

            template <typename TFunc, typename... TFutures>
            auto mbind(TFunc&& func, TFutures&&... futures) const&
            -> decltype(
                    future::mbind(
                        std::forward<TFunc>(func),
                        *this,
                        std::forward<TFutures>(futures)...
                        )
                    )
            {
                return future::mbind(
                        std::forward<TFunc>(func),
                        *this,
                        std::forward<TFutures>(futures)...
                        );
            }

            void listen(btl::MoveOnlyFunction<void(T)> callback)
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
                return FutureConnection(control_);
            }

            void setCallback_(std::function<void()> callback)
            {
                control_->addCallback(std::move(callback));
            }

            bool ready() const
            {
                return control_->ready();
            }

            operator Future<T const&>() const &
            {
                return { control_ };
            }

            operator Future<T const&>() &&
            {
                return { std::move(control_) };
            }

        private:
            std::shared_ptr<FutureControl<std::decay_t<T>>> control_;
        };
    } // future
} // btl

