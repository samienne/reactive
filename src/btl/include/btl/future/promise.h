#pragma once

#include "futurecontrol.h"
#include "futurebase.h"

#include <btl/visibility.h>

#include <exception>
#include <memory>

namespace btl::future
{
    /**
     * @brief The failure of a future whose last promise was destroyed
     * without being fulfilled.
     */
    class BTL_CLASS_VISIBLE BrokenPromise : public std::exception
    {
    public:
        char const* what() const noexcept override
        {
            return "broken promise";
        }
    };

    namespace detail
    {
        template <typename... Ts>
        class PromiseState
        {
        public:
            explicit PromiseState(std::weak_ptr<FutureControl<Ts...>> control) :
                control(std::move(control))
            {
            }

            PromiseState(PromiseState const&) = delete;
            PromiseState& operator=(PromiseState const&) = delete;

            ~PromiseState()
            {
                if (auto p = control.lock())
                {
                    p->setFailureIfPending(
                            std::make_exception_ptr(BrokenPromise()));
                }
            }

            std::weak_ptr<FutureControl<Ts...>> control;
        };
    } // namespace detail

    /**
     * @brief The producer side of a future.
     *
     * Copies share one producer. When the last copy is destroyed before the
     * future is fulfilled, the future fails with BrokenPromise.
     */
    template <typename... Ts>
    class Promise
    {
    public:
        Promise(std::weak_ptr<FutureControl<Ts...>> control) :
            state_(std::make_shared<detail::PromiseState<Ts...>>(
                        std::move(control)))
        {
        }

        bool valid() const
        {
            return state_ && !state_->control.expired();
        }

        void set(Ts... values)
        {
            if (auto p = lock())
                p->setValue(std::make_tuple(std::forward<Ts>(values)...));
        }

        void setFromTuple(std::tuple<Ts...> values)
        {
            if (auto p = lock())
                p->setValue(std::move(values));
        }

        void setFailure(std::exception_ptr err)
        {
            if (auto p = lock())
                p->setFailure(std::move(err));
        }

    private:
        std::shared_ptr<FutureControl<Ts...>> lock() const
        {
            if (!state_)
                return nullptr;

            return state_->control.lock();
        }

        std::shared_ptr<detail::PromiseState<Ts...>> state_;
    };
} // namespace btl::future
