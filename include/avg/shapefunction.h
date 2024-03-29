#pragma once

#include "drawcontext.h"
#include "shape.h"
#include "animationoptions.h"
#include "animated.h"

#include <btl/tuplereduce.h>

#include <chrono>

namespace avg
{
    namespace detail
    {
        class ShapeFunctionImpl
        {
        public:
            virtual ~ShapeFunctionImpl() = default;

            virtual Shape operator()(
                    DrawContext const& drawContext,
                    Vector2f size,
                    std::chrono::milliseconds time) const = 0;

            virtual bool isAnimationRunning(std::chrono::milliseconds time) const = 0;

            virtual std::shared_ptr<ShapeFunctionImpl> updated(
                    std::shared_ptr<ShapeFunctionImpl> const& old,
                    std::optional<AnimationOptions> animationOptions,
                    std::chrono::milliseconds time) = 0;
        };
    } // namespace detail

    class ShapeFunction
    {
    public:
        ShapeFunction(ShapeFunction const& rhs) = default;
        ShapeFunction(ShapeFunction&& rhs) noexcept = default;

        ShapeFunction& operator=(ShapeFunction const& rhs) = default;
        ShapeFunction& operator=(ShapeFunction&& rhs) noexcept = default;

        explicit ShapeFunction(
                std::shared_ptr<detail::ShapeFunctionImpl> impl) :
            impl_(std::move(impl))
        {
        }

        Shape operator()(
                DrawContext const& drawContext,
                Vector2f size,
                std::chrono::milliseconds time) const
        {
            return (*impl_)(drawContext, size, time);
        }

        auto updated(ShapeFunction const& old,
                std::optional<AnimationOptions> animationOptions,
                std::chrono::milliseconds time) const
        {
            return ShapeFunction(impl_->updated(old.impl_, animationOptions,
                        time));
        }

        bool isAnimationRunning(std::chrono::milliseconds time) const
        {
            return impl_->isAnimationRunning(time);
        }

    private:
        std::shared_ptr<detail::ShapeFunctionImpl> impl_;
    };

    namespace detail
    {
        template <typename TFunc, typename... Ts>
        class ShapeFunctionWithParams : public ShapeFunctionImpl
        {
        public:
            ShapeFunctionWithParams(TFunc func, std::tuple<Ts...> params) : 
                func_(std::move(func)),
                params_(std::move(params))
            {
            }

            Shape operator()(
                    DrawContext const& drawContext,
                    Vector2f size,
                    std::chrono::milliseconds time) const override
            {
                auto getAnimatedValue = [&](auto&& value) -> decltype(auto)
                {
                    if constexpr(IsAnimated<
                            std::decay_t<decltype(value)>
                            >::value)
                    {
                        return std::forward<decltype(value)>(value).getValue(time);
                    }
                    else
                    {
                        return std::forward<decltype(value)>(value);
                    }
                };

                return std::apply([&, this](auto&&... ts)
                    {
                        return func_(
                                drawContext,
                                size,
                                getAnimatedValue(std::forward<decltype(ts)>(ts))...
                             );
                    },
                    params_
                    );
            }

            std::shared_ptr<ShapeFunctionImpl> updated(
                    std::shared_ptr<ShapeFunctionImpl> const& newBase,
                    std::optional<AnimationOptions> animationOptions,
                    std::chrono::milliseconds time) override
            {
                auto const& newShape = static_cast<ShapeFunctionWithParams&>(*newBase);

                return std::make_shared<ShapeFunctionWithParams>(
                        newShape.func_,
                        updateParams(newShape.params_, time, animationOptions,
                            std::make_index_sequence<sizeof...(Ts)>()
                        ));
            }

            bool isAnimationRunning(std::chrono::milliseconds time) const override
            {
                return btl::tuple_reduce(false, params_,
                        [&](bool current, auto const& value)
                        {
                            // If Animated or a ShapeFunction
                            if constexpr(
                                    IsAnimated<std::decay_t<decltype(value)>>::value
                                    || std::is_same_v<ShapeFunction,
                                            std::decay_t<decltype(value)>>)
                            {
                                return current || value.isAnimationRunning(time);
                            }
                            else
                            {
                                return current;
                            }
                        });
            }

        private:
            template <size_t... S>
            auto updateParams(std::tuple<Ts...> const& newParams,
                    std::chrono::milliseconds time,
                    std::optional<AnimationOptions> animationOptions,
                    std::index_sequence<S...>) const
            {

                return std::make_tuple(
                        std::tuple_element_t<S, std::tuple<Ts...>>(
                            updateParam(
                                std::get<S>(params_),
                                std::get<S>(newParams),
                                animationOptions,
                                time
                                )
                            )...
                            );
            }

            template <typename T, typename U>
            static auto updateParam(T&& oldValue, U&& newValue,
                    std::optional<AnimationOptions> animationOptions,
                    std::chrono::milliseconds time)
            {
                if constexpr (IsAnimated<std::decay_t<T>>::value)
                {
                    return oldValue.updated(newValue, animationOptions, time);
                }
                else if constexpr(std::is_same_v<ShapeFunction, std::decay_t<T>>)
                {
                    return oldValue.updated(newValue, animationOptions, time);
                }
                else
                {
                    return std::forward<U>(newValue);
                }
            }

        private:
            TFunc func_;
            std::tuple<Ts...> params_;
        };

        template <typename TFunc, typename... Ts>
        auto makeShapeFunctionUnchecked(TFunc&& func, Ts&&... ts)
        {
            return ShapeFunction(
                std::make_shared<detail::ShapeFunctionWithParams<
                    std::decay_t<TFunc>,
                    std::decay_t<Ts>...
                    >>(std::forward<TFunc>(func),
                        std::make_tuple<std::decay_t<Ts>...>(
                            std::forward<Ts>(ts)...)));
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<
            Shape,
            TFunc,
            DrawContext const&,
            Vector2f,
            AnimatedTypeT<std::decay_t<Ts>>...
        >>>
    auto makeShapeFunction(TFunc&& func, Ts&&... ts)
    {
        return detail::makeShapeFunctionUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }
} // namespace avg
