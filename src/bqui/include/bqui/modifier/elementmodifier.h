#pragma once

#include "instancemodifier.h"

#include "bqui/provider/providebuildparams.h"
#include "bqui/provider/paramprovider.h"

#include "bqui/widget/element.h"

#include <btl/bindarguments.h>
#include <btl/cloneoncopy.h>

#include <type_traits>

namespace bqui::modifier
{
    template <typename TFunc>
    class ElementModifier;

    using AnyElementModifier = ElementModifier<
        std::function<widget::AnyElement(widget::AnyElement)>
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
        auto operator()(widget::Element<T> element)
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

    extern template class BQUI_EXPORT_TEMPLATE
        ElementModifier<std::function<widget::AnyElement(widget::AnyElement)>>;

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
                        provider::invokeParamProvider(
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
        std::is_invocable_r_v<widget::AnyElement, TFunc, widget::AnyElement,
            provider::ParamProviderTypeT<Ts>...>
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
        std::is_invocable_r_v<widget::AnyElement, TFunc, widget::AnyElement,
            provider::ParamProviderTypeT<Ts>...>
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
    widget::AnyElement operator|(widget::Element<T> element,
            ElementModifier<U> modifier)
    {
        return std::move(modifier)(
                std::move(element)
                );
    }

    template <typename T, typename U>
    widget::AnyElement operator|(widget::Element<T> element,
            InstanceModifier<U> modifier)
    {
        return makeElementModifier(std::move(modifier))(
                std::move(element)
                );
    }
}

