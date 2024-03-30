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
                    std::chrono::milliseconds time,
                    DrawContext const& drawContext,
                    Vector2f size) const = 0;

            virtual bool hasAnimationEnded(std::chrono::milliseconds time) const = 0;

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
                std::chrono::milliseconds time,
                DrawContext const& drawContext,
                Vector2f size) const
        {
            return (*impl_)(time, drawContext, size);
        }

        auto updated(ShapeFunction const& old,
                std::optional<AnimationOptions> animationOptions,
                std::chrono::milliseconds time) const
        {
            return ShapeFunction(impl_->updated(old.impl_, animationOptions,
                        time));
        }

        bool hasAnimationEnded(std::chrono::milliseconds time) const
        {
            return impl_->hasAnimationEnded(time);
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
                    std::chrono::milliseconds time,
                    DrawContext const& drawContext,
                    Vector2f size) const override
            {
                return std::apply([&, this](auto&&... ts)
                    {
                        return func_(
                                drawContext,
                                size,
                                std::forward<decltype(ts)>(ts)...
                             );
                    },
                    getAnimatedValue(params_, time)
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
                        getUpdatedAnimation(params_, newShape.params_,
                            animationOptions, time)
                        );
            }

            bool hasAnimationEnded(std::chrono::milliseconds time) const override
            {
                return avg::hasAnimationEnded(params_, time);
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
                        std::make_tuple(
                            std::forward<Ts>(ts)...)));
        }
    } // namespace detail

    template<>
    struct AnimatedTraits<ShapeFunction>
    {
        static auto getValue(ShapeFunction const& f,
                std::chrono::milliseconds time)
        {
            return [&f, time](DrawContext const& context, Vector2f size)
            {
                return f(time, context, size);
            };
        }

        static auto updated(ShapeFunction const& a, ShapeFunction const&b,
                std::optional<AnimationOptions> options,
                std::chrono::milliseconds time)
        {
            return a.updated(b, options, time);
        }

        static bool hasAnimationEnded(ShapeFunction const& f,
                std::chrono::milliseconds time)
        {
            return f.hasAnimationEnded(time);
        }
    };

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
