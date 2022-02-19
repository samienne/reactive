#pragma once

#include "controlwithdata.h"
#include "futureconnection.h"
#include "futurecontrol.h"
#include "futurebase.h"

#include <btl/moveonlyfunction.h>

namespace btl::future
{
    template <typename... Ts>
    class Future;

    template <typename... Ts>
    class SharedFuture
    {
    public:
        SharedFuture(std::shared_ptr<FutureControl<std::decay_t<Ts>...>> control) :
            control_(std::move(control))
        {
        }

        SharedFuture(SharedFuture const& rhs) :
            control_(rhs.control_)
        {
        }

        SharedFuture(SharedFuture&&) noexcept = default;

        SharedFuture& operator=(SharedFuture const& rhs)
        {
            control_ = rhs.control_;
            return *this;
        }

        SharedFuture& operator=(SharedFuture&&) noexcept = default;

        auto get() const -> decltype(auto)
        {
            return control_->template get<std::decay_t<Ts> const&...>();
        }

        auto getTuple() const -> decltype(auto)
        {
            return control_->template getAsTuple<std::decay_t<Ts> const&...>();
        }

        template <typename TFunc, typename = std::enable_if_t<
            std::is_invocable_v<TFunc, FutureValueTypeT<Ts const&>...>
            >>
        auto then(TFunc&& func) const
        {
            Future<std::decay_t<Ts> const&...> copy(control_);

            return std::move(copy).then(std::forward<TFunc>(func));
        }

        operator Future<std::decay_t<Ts> const&...>() const
        {
            return { control_ };
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
                        std::apply(
                            std::move(cb),
                            p->template getAsTuple<Ts&&...>()
                            );
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

        auto share() &&
        {
            return std::move(*this);
        }

    private:
        template <typename... Us>
        friend class Future;

        std::shared_ptr<FutureControl<std::decay_t<Ts>...>> control_;
    };
} // namespace btl::future

