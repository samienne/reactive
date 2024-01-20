#pragma once

#include "providebuildparams.h"
#include "paramprovider.h"
#include "instancemodifier.h"
#include "element.h"

#include <btl/bindarguments.h>
#include <btl/cloneoncopy.h>

#include <type_traits>

namespace reactive::widget
{
    template <typename TFunc>
    class ElementModifier;

    using AnyElementModifier = ElementModifier<
        std::function<AnyElement(AnyElement)>
        >;

    template <typename TFunc>
    class ElementModifier
    {
    public:
        ElementModifier(TFunc func) :
            func_(std::move(func))
        {
        }

        ElementModifier(ElementModifier const&) = default;
        ElementModifier(ElementModifier&&) noexcept = default;

        ElementModifier& operator=(ElementModifier const&) = default;
        ElementModifier& operator=(ElementModifier&&) noexcept = default;

        template <typename T>
        auto operator()(Element<T> element)
        {
            return std::invoke(*func_, std::move(element));
        }

        operator AnyElementModifier() &&
        {
            return AnyElementModifier(std::move(*func_));
        }

    private:
        btl::CloneOnCopy<TFunc> func_;
    };

    extern template class REACTIVE_EXPORT_TEMPLATE
        ElementModifier<std::function<AnyElement(AnyElement)>>;

    namespace detail
    {
        template <typename TFunc>
        ElementModifier<std::decay_t<TFunc>> makeElementModifierUnchecked(
                TFunc&& func)
        {
            return ElementModifier<std::decay_t<TFunc>>(
                    std::forward<TFunc>(func)
                    );
        }

        struct MakeElementModifierUnchecked1
        {
            template <typename T, typename TFunc, typename... Ts>
            auto operator()(T&& element, TFunc&& func, Ts&&... ts) const
            {
                auto params = element.getParams();

                return std::invoke(
                        std::forward<decltype(func)>(func),
                        std::forward<T>(element),
                        invokeParamProvider(
                            std::forward<decltype(ts)>(ts),
                            params
                            )...
                        );
            }
        };

        template <typename TFunc, typename... Ts>
        auto makeElementModifierUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeElementModifierUnchecked(btl::bindArguments(
                        detail::MakeElementModifierUnchecked1(),
                        std::forward<TFunc>(func),
                        std::forward<Ts>(ts)...
                        )
                    );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyElement, TFunc, AnyElement, ParamProviderTypeT<Ts>...>
        >>
    auto makeElementModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeElementModifierUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    namespace detail
    {
        struct MakeSharedElementModifier1
        {
            template <typename T, typename TFunc, typename... Ts>
            auto operator()(T&& element, TFunc&& func, Ts&&... ts) const
            {
                return std::invoke(
                        std::forward<decltype(func)>(func),
                        std::forward<T>(element).share(),
                        std::forward<decltype(ts)>(ts)...
                        );
            }
        };
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyElement, TFunc, AnyElement, ParamProviderTypeT<Ts>...>
        >>
    auto makeSharedElementModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeElementModifierUnchecked(
                detail::MakeSharedElementModifier1(),
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    namespace detail
    {
        struct MakeElementModifier1
        {
            template <typename T, typename U>
            auto operator()(T&& element, U&& modifier) const
            {
                auto params = element.getParams();

                return makeElement(
                        std::forward<T>(element).getInstance()
                            | std::forward<decltype(modifier)>(modifier),
                        std::move(params)
                        );
            }
        };
    } // namespace detail

    template <typename T>
    auto makeElementModifier(InstanceModifier<T> modifier)
    {
        return detail::makeElementModifierUnchecked(
                detail::MakeElementModifier1(),
                std::move(modifier)
                );
    }

    template <typename T, typename U>
    AnyElement operator|(Element<T> element, ElementModifier<U> modifier)
    {
        return std::move(modifier)(
                std::move(element)
                );
    }

    template <typename T, typename U>
    AnyElement operator|(Element<T> element, InstanceModifier<U> modifier)
    {
        return makeElementModifier(std::move(modifier))(
                std::move(element)
                );
    }
} // namespace reactive::widget

