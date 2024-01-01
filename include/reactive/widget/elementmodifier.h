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

        template <typename TFunc, typename... Ts>
        auto makeElementModifierUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeElementModifierUnchecked(btl::bindArguments(
                        [](auto element, auto&& func, auto&&... ts)
                        {
                            auto params = element.getParams();

                            return std::invoke(
                                    std::forward<decltype(func)>(func),
                                    std::move(element),
                                    invokeParamProvider(
                                        std::forward<decltype(ts)>(ts),
                                        params
                                        )...
                                    );
                        },
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

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyElement, TFunc, AnyElement, ParamProviderTypeT<Ts>...>
        >>
    auto makeSharedElementModifier(TFunc&& func, Ts&&... ts)
    {
        return detail::makeElementModifierUnchecked(
                [](auto element, auto&& func, auto&&... ts)
                {
                    return std::invoke(
                            std::forward<decltype(func)>(func),
                            std::move(element).share(),
                            std::forward<decltype(ts)>(ts)...
                            );
                },
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }

    template <typename T>
    auto makeElementModifier(InstanceModifier<T> modifier)
    {
        return detail::makeElementModifierUnchecked(
            [](auto element, auto&& modifier)
            {
                auto params = element.getParams();

                return makeElement(
                        std::move(element).getInstance()
                            | std::forward<decltype(modifier)>(modifier),
                        std::move(params)
                        );
            },
            std::move(modifier)
            );
    }

    template <typename T, typename U>
    auto operator|(Element<T> element, ElementModifier<U> modifier)
    {
        return std::invoke(
                std::move(modifier),
                std::move(element)
                );
    }

    template <typename T, typename U>
    auto operator|(Element<T> element, InstanceModifier<U> modifier)
    {
        return std::invoke(
                makeElementModifier(std::move(modifier)),
                std::move(element)
                );
    }
} // namespace reactive::widget

