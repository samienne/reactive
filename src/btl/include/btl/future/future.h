#pragma once

#include "sharedfuture.h"
#include "futureconnection.h"
#include "futurecontrol.h"
#include "futurebase.h"
#include "futureresult.h"

#include <btl/all.h>
#include <btl/moveonlyfunction.h>
#include <btl/foreach.h>
#include <btl/tupleforeach.h>
#include <btl/tuplemap.h>
#include <btl/future/controlwithdata.h>

#include <exception>
#include <tuple>
#include <memory>
#include <type_traits>

namespace btl::future
{
    template <typename T, template <typename... Us> typename U, typename... Ws>
    struct ApplyParamsFrom
    {
        using type = void;
    };

    template <template <typename... Vs> typename T, template <typename... Us> typename U,
             typename... Ws, typename... Vs>
    struct ApplyParamsFrom<T<Vs...>, U, Ws...>
    {
        using type = U<Ws..., Vs...>;
    };

    template <typename T, template <typename... Us> typename U, typename... Ws>
    using ApplyParamsFromT = typename ApplyParamsFrom<T, U, Ws...>::type;

    static_assert(std::is_same_v<
            FutureControl<int, char>,
            ApplyParamsFromT<std::tuple<int, char>, FutureControl>
            >);

    template <typename... Ts>
    class Future;

    template <typename... Ts>
    class SharedFuture;

    template <typename T>
    struct IsFuture : std::false_type {};

    template <typename... Ts>
    struct IsFuture<Future<Ts...>> : std::true_type {};

    template <typename... Ts>
    struct IsFuture<SharedFuture<Ts...>> : std::true_type {};

    template <typename... Ts>
    class Future
    {
    public:
        using ControlType = FutureControl<std::decay_t<Ts>...>;

        Future(std::shared_ptr<FutureControl<std::decay_t<Ts>...>> base) :
            control_(std::move(base))
        {
        }

        Future(SharedFuture<Ts...>& rhs) :
            control_(rhs.control_)
        {
        };

        Future(SharedFuture<Ts...> const& rhs) :
            control_(rhs.control_)
        {
        };

        Future(SharedFuture<Ts...>&& rhs) :
            control_(std::move(rhs.control_))
        {
        };

        Future(Future const&) = delete;
        Future(Future&&) noexcept = default;

        Future& operator=(Future const&) = delete;
        Future& operator=(Future&&) noexcept = default;

        Future& operator=(SharedFuture<Ts...> const& rhs)
        {
            control_ = rhs.control_;
            return *this;
        }

        Future& operator=(SharedFuture<Ts...>&& rhs)
        {
            control_ = std::move(rhs.control_);
            return *this;
        }

        auto get() && -> decltype(auto)
        {
            return control_->template get<Ts...>();
        }

        auto getTuple() && -> decltype(auto)
        {
            return control_->template getAsTuple<Ts&&...>();
        }

        auto getRefTuple() -> decltype(auto)
        {
            return control_->template getAsTuple<Ts&...>();
        }

        template <typename TFunc, typename = std::enable_if_t<
            std::is_invocable_v<TFunc, std::exception_ptr>
            >>
        auto onFailure(TFunc callback) &&
        {
            control_->addCallback(
                [callback=std::move(callback)](auto& control) mutable
                {
                    if (control.hasException())
                        callback(control.getException());
                });

            return std::move(*this);
        }

        template <typename TFunc, typename = std::enable_if_t<
            std::is_invocable_v<TFunc, FutureValueTypeT<Ts>...>
            >>
        auto then(TFunc&& func) &&
        {
            using ReturnType = std::decay_t<std::invoke_result_t<
                TFunc,
                FutureValueTypeT<Ts>...
                >>;
            using ValueType = FutureValueTypeT<ReturnType>;

            using DataType = std::pair<FutureConnection, std::decay_t<TFunc>>;

            using FutureControlType = std::conditional_t<
                IsFutureResult<std::decay_t<ValueType>>::value,
                ApplyParamsFromT<ValueType, detail::ControlWithData, DataType>,
                std::conditional_t<
                    std::is_same_v<void, ValueType>,
                    detail::ControlWithData<DataType>,
                    detail::ControlWithData<DataType, std::decay_t<ValueType>>
                >>;

            using FutureType = std::conditional_t<
                IsFutureResult<std::decay_t<ValueType>>::value,
                ApplyParamsFromT<std::decay_t<ValueType>, Future>,
                std::conditional_t<
                    std::is_same_v<void, ValueType>,
                    Future<>,
                    Future<ValueType>
                >>;

            auto control = std::make_shared<FutureControlType>(
                    std::make_pair(connect(), std::forward<TFunc>(func))
                    );
            std::weak_ptr<FutureControlType> weakControl = control;

            control_->addCallback(
                [newControl=std::move(weakControl)](auto& control) mutable
                {
                    if (auto p = newControl.lock())
                    {
                        try
                        {
                            if constexpr(std::is_same_v<void, ValueType>)
                            {
                                std::apply(
                                        std::move(p->data.second),
                                        std::move(control.getTupleRef())
                                        );

                                p->setValue(std::tuple<>());
                            }
                            else
                            {
                                auto r = std::apply(
                                        std::move(p->data.second),
                                        std::move(control.getTupleRef())
                                        );

                                if constexpr (IsFuture<std::decay_t<decltype(r)>>::value)
                                {
                                    auto f = std::move(r).then(
                                        [newControl=newControl](auto&&... ts) mutable
                                        {
                                            if (auto p = newControl.lock())
                                            {
                                                p->setValue(std::forward_as_tuple(
                                                            std::forward<decltype(ts)>(ts)...
                                                            ));
                                            }
                                        }).onFailure(
                                        [newControl=std::move(newControl)](std::exception_ptr err)
                                        {
                                            if (auto p = newControl.lock())
                                            {
                                                p->setFailure(std::move(err));
                                            }
                                        });

                                    p->data.first = std::move(f).connect();
                                }
                                else if constexpr (IsFutureResult<std::decay_t<decltype(r)>>::value)
                                {
                                    p->setValue(std::move(r).getAsTuple());
                                }
                                else
                                {
                                    p->setValue(std::make_tuple(std::move(r)));
                                }
                            }
                        }
                        catch(...)
                        {
                            p->setFailure(std::current_exception());
                        }
                    }
                });

            return FutureType(std::move(control));
        }

        FutureConnection connect() const
        {
            return FutureConnection(std::move(control_));
        }

        void addCallback_(std::function<void(ControlType&)> callback)
        {
            assert(control_);
            control_->addCallback(std::move(callback));
        }

        bool ready() const
        {
            return control_->ready();
        }

        SharedFuture<std::decay_t<Ts> const&...> share() &&
        {
            return { std::move(control_) };
        }

        template <typename... Us, typename = std::enable_if_t<
            btl::All<
                std::is_convertible<Ts, Us>...
            >::value
            >>
        operator Future<Us...>() &&
        {
            if (control_.use_count() == 1)
                return { std::move(control_) };

            using FutureControlType =
                    detail::ControlWithData
                    <
                        std::shared_ptr<FutureControl<std::decay_t<Ts>...>>,
                        std::decay_t<Us>...
                    >;

            auto control = std::make_shared<FutureControlType>(
                    std::move(control_));

            std::weak_ptr<FutureControlType> weakControl = control;

            control->data->addCallback([weakControl=weakControl](auto& control) mutable
                {
                    if (auto p = weakControl.lock())
                    {
                        if (control.hasValue())
                            p->setValue(control.template get<Us...>());
                        else
                            p->setFailure(control.getException());
                    }
                });

            return { std::move(control) };
        }

        void wait() const&
        {
            control_->waitForResult();
        }

        void detach() &&
        {
            auto control = control_;
            control_->addCallback([control=std::move(control)](auto&) mutable
                {
                });
        }

        void cancel() &&
        {
            control_.reset();
        }

    protected:
        std::shared_ptr<ControlType> control_;
    };

    template <typename... Ts>
    auto makeReadyFuture(Ts&&... ts) -> Future<std::decay_t<Ts>...>
    {
        auto control = std::make_shared<FutureControl<std::decay_t<Ts>...>>();
        control->setValue(std::forward<Ts>(ts)...);
        return Future<std::decay_t<Ts>...>(std::move(control));
    }
} // namespace btl::future

