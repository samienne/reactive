#pragma once

#include "futureconnection.h"
#include "futurecontrol.h"
#include "futurebase.h"
#include "mbind.h"
#include "fmap.h"

#include <btl/moveonlyfunction.h>

namespace btl::future
{
    template <typename... Ts>
    class Future;

    template <typename... Ts>
    class SharedFuture
    {
    public:
        SharedFuture(std::shared_ptr<FutureControl<std::decay_t<Ts>...>> base) :
            control_(std::move(base))
        {
        }

        SharedFuture(SharedFuture const&) = default;
        SharedFuture(SharedFuture&&) noexcept = default;

        SharedFuture& operator=(SharedFuture const&) = default;
        SharedFuture& operator=(SharedFuture&&) noexcept = default;

        auto get() const -> decltype(auto)
        {
            return control_->template get<std::decay_t<Ts> const&...>();
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

        void listen(btl::MoveOnlyFunction<void(Ts...)> callback)
        {
            std::weak_ptr<FutureControl<std::decay_t<Ts>...>> weak(
                    std::move(control_));

            control_->addCallback(
                    [cb=std::move(callback), control=std::move(weak)]()
                    mutable
                {
                    if (auto p = control.lock())
                    {
                        std::move(cb)(p->template get<Ts...>());
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

        operator Future<Ts const&...>() const &
        {
            return { control_ };
        }

        operator Future<Ts const&...>() &&
        {
            return { std::move(control_) };
        }

    private:
        std::shared_ptr<FutureControl<std::decay_t<Ts>...>> control_;
    };
} // namespace btl::future

