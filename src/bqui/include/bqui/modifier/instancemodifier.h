#pragma once

#include "bqui/widget/instance.h"

#include <btl/bindarguments.h>

#include <type_traits>

namespace bqui::modifier
{
    template <typename TFunc>
    class InstanceModifier;

    using AnyInstanceModifier = InstanceModifier<
        std::function<bq::signal::AnySignal<widget::Instance>(
                bq::signal::AnySignal<widget::Instance>)>
        >;

    template <typename T>
    struct IsInstanceModifier : std::false_type {};

    template <typename T>
    struct IsInstanceModifier<InstanceModifier<T>> : std::true_type {};

    struct InstanceModifierBuildTag {};

    template <typename TFunc>
    class InstanceModifier
    {
    public:
        InstanceModifier(InstanceModifierBuildTag&&, TFunc&& func) :
            func_(std::move(func))
        {
        }

        InstanceModifier(InstanceModifier const&) = default;
        InstanceModifier(InstanceModifier&&) = default;

        InstanceModifier& operator=(InstanceModifier const&) = default;
        InstanceModifier& operator=(InstanceModifier&&) = default;

        template <typename U>
        auto operator()(bq::signal::Signal<U, widget::Instance> instance)
        {
            return func_(std::move(instance));
        }

        operator AnyInstanceModifier() &&
        {
            return AnyInstanceModifier(
                    InstanceModifierBuildTag{},
                    std::move(func_)
                    );
        }

    private:
        TFunc func_;
    };

    template <typename T, typename U>
    auto operator|(bq::signal::Signal<T, widget::Instance> w,
            InstanceModifier<U> t)
    {
        return std::move(std::move(t)(std::move(w)));
    }

    namespace detail
    {
        template <typename TFunc>
        auto makeInstanceSignalModifierUnchecked(TFunc&& f)
        {
            return InstanceModifier<std::decay_t<TFunc>>(
                    InstanceModifierBuildTag{},
                    std::forward<TFunc>(f)
                    );
        }

        struct MakeInstanceModifierUnchecked1
        {
            template <typename T, typename TFunc, typename... Ts>
            auto operator()(T&& instance, TFunc&& func, Ts&&... ts) const
            {
                return std::invoke(
                        std::forward<decltype(func)>(func),
                        std::forward<decltype(instance)>(instance),
                        std::forward<decltype(ts)>(ts)...
                        );
            }
        };

        template <typename TFunc, typename... Ts>
        auto makeInstanceSignalModifierUnchecked(TFunc&& f, Ts&&... ts)
        {
            return makeInstanceSignalModifierUnchecked(
                    btl::bindArguments(
                        detail::MakeInstanceModifierUnchecked1(),
                        std::forward<TFunc>(f),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts,
             typename = std::enable_if_t<std::is_invocable_r_v<
                 bq::signal::AnySignal<widget::Instance>, TFunc,
                     bq::signal::AnySignal<widget::Instance>, Ts&&...
                 > > >
    auto makeInstanceSignalModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeInstanceSignalModifierUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    namespace detail
    {
        struct MakeSharedInstanceSignalModifier1
        {
            template <typename T, typename TFunc, typename... Ts>
            auto operator()(T&& instance, TFunc&& f, Ts&&... ts) const
            {
                return std::invoke(
                        std::forward<decltype(f)>(f),
                        std::forward<T>(instance).share(),
                        std::forward<decltype(ts)>(ts)...
                        );
            }
        };
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_convertible_v<
            TFunc,
            std::function<bq::signal::AnySignal<widget::Instance>(
                    bq::signal::AnySignal<widget::Instance>, Ts...)>>
        >>
    auto makeSharedInstanceSignalModifier(TFunc&& f, Ts&&... ts)
    {
        return detail::makeInstanceSignalModifierUnchecked(
                detail::MakeSharedInstanceSignalModifier1(),
                std::forward<TFunc>(f),
                std::forward<Ts>(ts)...
                );
    }

    namespace detail
    {
        template <typename TFunc>
        struct MakeInstanceModifier1
        {
            template <typename T, typename... Ts>
            auto operator()(T&& instance, Ts&&... ts)
            {
                return merge(std::forward<T>(instance),
                        std::forward<decltype(ts)>(ts)...
                        ).map(func);
            }

            TFunc func;
        };
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        btl::isInvocableRV<
            widget::Instance,
            TFunc,
            widget::Instance,
            bq::signal::UnpackSignalResultT<bq::signal::SignalTypeT<Ts>>...
        >>>
    auto makeInstanceModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeInstanceSignalModifierUnchecked(
                detail::MakeInstanceModifier1<std::decay_t<TFunc>>{
                std::forward<TFunc>(func)
                },
                std::forward<Ts>(ts)...
            );
    }

    namespace detail
    {
        struct MakeEmptyInstanceModifier
        {
            template <typename T>
            auto operator()(T&& instance) const
            {
                return std::forward<T>(instance);
            }
        };
    } // namespace detail

    inline auto makeEmptyInstanceModifier()
    {
        return detail::makeInstanceSignalModifierUnchecked(
                detail::MakeEmptyInstanceModifier()
                );
    }
}

