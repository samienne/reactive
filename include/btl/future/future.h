#pragma once

#include "btl/future/controlwithdata.h"
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

            using ControlType = std::conditional_t<
                IsFutureResult<std::decay_t<ValueType>>::value,
                ApplyParamsFromT<ValueType, detail::ControlWithData, DataType>,
                detail::ControlWithData<DataType, std::decay_t<ValueType>>
                >;

            using FutureType = std::conditional_t<
                IsFutureResult<std::decay_t<ValueType>>::value,
                ApplyParamsFromT<std::decay_t<ValueType>, Future>,
                Future<ValueType>
                >;

            auto control = std::make_shared<ControlType>(
                    std::make_pair(connect(), std::forward<TFunc>(func))
                    );
            std::weak_ptr<ControlType> weakControl = control;

            std::move(*this).listen(
                [control=std::move(weakControl)](auto&&... ts) mutable
                {
                    if (auto p = control.lock())
                    {
                        auto r = std::invoke(
                                std::move(p->data.second),
                                std::forward<decltype(ts)>(ts)...
                                );

                        if constexpr (IsFuture<std::decay_t<decltype(r)>>::value)
                        {
                            auto f = std::move(r).then(
                                [control=std::move(control)](auto&&... ts) mutable
                                {
                                    if (auto p = control.lock())
                                    {
                                        p->set(std::forward_as_tuple(
                                                    std::forward<decltype(ts)>(ts)...
                                                    ));
                                    }

                                    return true;
                                });

                            p->data.first = std::move(f).connect();
                        }
                        else if constexpr (IsFutureResult<std::decay_t<decltype(r)>>::value)
                        {
                            p->set(std::move(r).getAsTuple());
                        }
                        else
                        {
                            p->set(std::make_tuple(std::move(r)));
                        }
                    }
                });

            return FutureType(std::move(control));
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
            if (control_.unique())
                return { std::move(control_) };

            using ControlType =
                    detail::ControlWithData
                    <
                        std::shared_ptr<FutureControl<std::decay_t<Ts>...>>,
                        std::decay_t<Us>...
                    >;

            auto control = std::make_shared<ControlType>(
                    std::move(control_));

            std::weak_ptr<ControlType> weakControl = control;

            control->data->addCallback([control=weakControl]() mutable
                {
                    if (auto p = control.lock())
                    {
                        p->set(p->data->template get<Us...>());
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

    protected:
        std::shared_ptr<FutureControl<std::decay_t<Ts>...>> control_;
    };

    template <typename... Ts>
    auto makeReadyFuture(Ts&&... ts) -> Future<std::decay_t<Ts>...>
    {
        auto control = std::make_shared<FutureControl<std::decay_t<Ts>...>>();
        control->set(std::forward<Ts>(ts)...);
        return Future<std::decay_t<Ts>...>(std::move(control));
    }
} // namespace btl::future

