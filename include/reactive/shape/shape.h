#pragma once

#include <reactive/animate.h>

#include <reactive/widget/ondraw.h>
#include <reactive/widget/widget.h>

#include <reactive/signal/fromoptional.h>

#include <avg/animated.h>
#include <avg/brush.h>
#include <avg/drawcontext.h>
#include <avg/path.h>
#include <avg/pen.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/vector.h>

#include <type_traits>

namespace reactive::shape
{
    struct ShapeBuildTag {};

    namespace detail
    {
        template <typename T>
        auto makeShapeUncheckedFromSignal(signal::Signal<T, avg::ShapeFunction> func);
    }

    template <typename T>
    class Shape;

    using AnyShape = Shape<signal::AnySignal<avg::ShapeFunction>>;

    template <typename T, typename U, typename V>
    auto makeDrawModifier(
            signal::Signal<T, avg::ShapeFunction> func,
            signal::Signal<U, std::optional<avg::Brush>> brush,
            signal::Signal<V, std::optional<avg::Pen>> pen
            )
    {
        return widget::onDraw([](avg::DrawContext const& context,
                        avg::Vector2f size,
                        auto func,
                        std::optional<avg::Brush> const& brush,
                        std::optional<avg::Pen> const& pen)
                {
                    return func(context, size)
                        .fillAndStroke(
                                std::move(brush),
                                std::move(pen)
                                );
                },
                std::move(func),
                reactive::animate(std::move(brush)),
                reactive::animate(std::move(pen))
                );
    }

    template <typename T>
    class Shape
    {
    public:
        Shape(ShapeBuildTag const&, T func) :
            func_(std::move(func))
        {
        }

        template <typename U>
        auto stroke(signal::Signal<U, avg::Pen> pen) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(
                        func_.clone(),
                        signal::constant(std::optional<avg::Brush>()),
                        std::move(pen).map([](auto p)
                            {
                                return std::make_optional(std::move(p));
                            }))
                ;
        }

        auto stroke(avg::Pen pen) &&
        {
            return stroke(signal::constant(std::move(pen)));
        }

        template <typename U>
        auto fill(signal::Signal<U, avg::Brush> brush) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(func_.clone(),
                        std::move(brush).map([](auto b)
                            {
                                return std::make_optional(std::move(b));
                            }),
                        signal::constant(std::optional<avg::Pen>())
                        )
                ;
        }

        auto fill(avg::Brush brush) &&
        {
            return fill(std::move(brush));
        }

        auto fill(avg::Color const& color) &&
        {
            return fill(avg::Brush(color));
        }

        template <typename U>
        auto strokeToShape(signal::Signal<U, avg::Pen> pen) && // -> Shape
        {
            return detail::makeShapeUncheckedFromSignal(
                    merge(std::move(func_), std::move(pen)).map(
                        [](avg::ShapeFunction func, avg::Pen pen)
                        {
                            return makeShapeFunction(
                                [](avg::DrawContext const& drawContext,
                                    avg::Vector2f size,
                                    auto func,
                                    avg::Pen pen)
                                {
                                    return func(drawContext, size)
                                        .strokeToShape(pen);
                                },
                                std::move(func),
                                pen
                                );
                        }));
        }

        widget::AnyWidget fillAndStroke(
                std::optional<signal::AnySignal<avg::Brush>> brush,
                std::optional<signal::AnySignal<avg::Pen>> pen) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(
                        func_.clone(),
                        signal::fromOptional(std::move(brush)),
                        signal::fromOptional(std::move(pen)))
                ;
        }

        auto clip(Shape clipShape) // -> Shape
        {
            return detail::makeShapeUncheckedFromSignal(
                    merge(std::move(func_), std::move(clipShape.func_))
                    .map([](auto lhs, auto rhs)
                        {
                            return makeShapeFunction(
                                [](avg::DrawContext const& drawContext,
                                    avg::Vector2f size,
                                    auto lhs,
                                    auto rhs)
                                {
                                    return lhs(drawContext, size)
                                        & rhs(drawContext, size);
                                },
                                std::move(lhs),
                                std::move(rhs)
                                );
                        }));
        }

        operator AnyShape() const&
        {
            return AnyShape(ShapeBuildTag{}, func_);
        }

        operator AnyShape() &&
        {
            return AnyShape(ShapeBuildTag{}, std::move(func_));
        }

    private:
        T func_;
    };

    namespace detail
    {
        template <typename T>
        auto makeShapeUncheckedFromSignal(
                signal::Signal<T, avg::ShapeFunction> func)
        {
            return Shape<signal::Signal<T, avg::ShapeFunction>>(
                    ShapeBuildTag{},
                    std::move(func));
        }

        template <typename TFunc, typename... Ts>
        auto makeShapeUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeShapeUncheckedFromSignal(
                merge(std::forward<Ts>(ts)...).map(
                        [func=std::forward<TFunc>(func)](auto&&... values) mutable
                        {
                            return makeShapeFunction(
                                    func,
                                    std::forward<decltype(values)>(values)...
                                    );
                        })
                );
        }
    } // namespace detail

    template <typename TFunc, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<
            avg::Shape,
            TFunc,
            avg::DrawContext const&,
            avg::Vector2f,
            avg::AnimatedTypeT<std::decay_t<
                signal::SingleSignalTypeT<widget::ParamProviderTypeT<std::decay_t<Ts>>>
            >>...
        >>>
    auto makeShape(TFunc&& func, Ts&&... ts)
    {
        return detail::makeShapeUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }
} // namespace reactive::shape
