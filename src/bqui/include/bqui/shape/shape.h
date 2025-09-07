#pragma once

#include <bqui/animate.h>

#include <bqui/modifier/ondraw.h>

#include <bqui/widget/widget.h>

#include <bq/signal/fromoptional.h>

#include <avg/animated.h>
#include <avg/brush.h>
#include <avg/drawcontext.h>
#include <avg/path.h>
#include <avg/pen.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/vector.h>

#include <type_traits>

namespace bqui::shape
{
    struct ShapeBuildTag {};

    namespace detail
    {
        template <typename T>
        auto makeShapeUncheckedFromSignal(bq::signal::Signal<T, avg::ShapeFunction> func);
    }

    template <typename T>
    class Shape;

    using AnyShape = Shape<bq::signal::AnySignal<avg::ShapeFunction>>;

    template <typename T, typename U, typename V>
    auto makeDrawModifier(
            bq::signal::Signal<T, avg::ShapeFunction> func,
            bq::signal::Signal<U, std::optional<avg::Brush>> brush,
            bq::signal::Signal<V, std::optional<avg::Pen>> pen
            )
    {
        return modifier::onDraw([](avg::DrawContext const& context,
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
                animate(std::move(brush)),
                animate(std::move(pen))
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
        auto stroke(bq::signal::Signal<U, avg::Pen> pen) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(
                        func_.clone(),
                        bq::signal::constant(std::optional<avg::Brush>()),
                        std::move(pen).map([](auto p)
                            {
                                return std::make_optional(std::move(p));
                            }))
                ;
        }

        auto stroke(avg::Pen pen) &&
        {
            return stroke(bq::signal::constant(std::move(pen)));
        }

        template <typename U>
        auto fill(bq::signal::Signal<U, avg::Brush> brush) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(func_.clone(),
                        std::move(brush).map([](auto b)
                            {
                                return std::make_optional(std::move(b));
                            }),
                        bq::signal::constant(std::optional<avg::Pen>())
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
        auto strokeToShape(bq::signal::Signal<U, avg::Pen> pen) && // -> Shape
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
                std::optional<bq::signal::AnySignal<avg::Brush>> brush,
                std::optional<bq::signal::AnySignal<avg::Pen>> pen) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(
                        func_.clone(),
                        bq::signal::fromOptional(std::move(brush)),
                        bq::signal::fromOptional(std::move(pen)))
                ;
        }

        auto clip(Shape clipShape) && // -> Shape
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

        auto size(bq::signal::AnySignal<avg::Vector2f> size,
                bq::signal::AnySignal<avg::Vector2f> gravity) && // -> Shape
        {
            return detail::makeShapeUncheckedFromSignal(
                merge(std::move(func_), std::move(size), std::move(gravity))
                .map([](auto func, avg::Vector2f size, avg::Vector2f gravity)
                {
                    return makeShapeFunction(
                        [](avg::DrawContext const& drawContext,
                            avg::Vector2f size,
                            auto func,
                            avg::Vector2f requestedSize,
                            avg::Vector2f gravity) -> avg::Shape
                        {
                            avg::Vector2f offset = (requestedSize - size);

                            offset = {
                                offset.x() * gravity.x(),
                                offset.y() * gravity.y()
                            };

                            return std::move(func(drawContext, requestedSize))
                                .transform(avg::translate(-offset))
                                ;
                        },
                        std::move(func),
                        std::move(size),
                        std::move(gravity)
                        );
                }));
        }

        auto size(bq::signal::AnySignal<avg::Vector2f> size) && // -> Shape
        {
            return std::move(*this).size(
                    std::move(size),
                    bq::signal::constant(avg::Vector2f(0.5f, 0.5f))
                    );
        }

        auto transform(bq::signal::AnySignal<avg::Animated<avg::Transform>> transform) && // -> Shape
        {
            return detail::makeShapeUncheckedFromSignal(
                merge(std::move(func_), std::move(transform))
                .map([](auto func, avg::Animated<avg::Transform> transform)
                {
                    return makeShapeFunction(
                        [](avg::DrawContext const& drawContext,
                            avg::Vector2f size,
                            auto func,
                            avg::Transform transform) -> avg::Shape
                        {
                            return std::move(func(drawContext, size))
                                .transform(std::move(transform))
                                ;
                        },
                        std::move(func),
                        std::move(transform)
                        );
                }));
        }

        auto transform(avg::Transform transform) && // -> Shape
        {
            return std::move(*this).transform(
                    bq::signal::constant(std::move(transform))
                    );
        }

        auto translate(bq::signal::AnySignal<avg::Animated<avg::Vector2f>> offset)
        {
            return detail::makeShapeUncheckedFromSignal(
                merge(std::move(func_), std::move(offset))
                .map([](auto func, avg::Animated<avg::Vector2f> offset)
                {
                    return makeShapeFunction(
                        [](avg::DrawContext const& drawContext,
                            avg::Vector2f size,
                            auto func,
                            avg::Vector2f offset) -> avg::Shape
                        {
                            return std::move(func(drawContext, size))
                                .translate(offset)
                                ;
                        },
                        std::move(func),
                        std::move(offset)
                        );
                }));
        }

        auto translate(avg::Vector2f translation) && // -> Shape
        {
            return std::move(*this).translate(bq::signal::constant(translation));
        }

        auto rotate(bq::signal::AnySignal<avg::Animated<float>> angle,
                bq::signal::AnySignal<avg::Animated<avg::Vector2f>> anchor =
                bq::signal::constant(avg::Vector2f(0.5f, 0.5f))) && // -> Shape
        {
            return detail::makeShapeUncheckedFromSignal(
                merge(std::move(func_), std::move(angle), std::move(anchor))
                .map([](auto func, avg::Animated<float> angle,
                        avg::Animated<avg::Vector2f> anchor)
                {
                    return makeShapeFunction(
                        [](avg::DrawContext const& drawContext,
                            avg::Vector2f size,
                            auto func,
                            float angle,
                            avg::Vector2f anchor) -> avg::Shape
                        {
                            avg::Vector2f offset(
                                    size.x() * anchor.x(),
                                    size.y() * anchor.y()
                                    );

                            return std::move(func(drawContext, size))
                                .translate(-offset)
                                .rotate(angle)
                                .translate(offset)
                                ;
                        },
                        std::move(func),
                        std::move(angle),
                        std::move(anchor)
                        );
                }));
        }

        auto rotate(float angle, avg::Vector2f anchor = { 0.5f, 0.5f }) && // -> Shape
        {
            return std::move(*this).rotate(
                    bq::signal::constant(angle),
                    bq::signal::constant(anchor)
                    );
        }

        auto scale(bq::signal::AnySignal<avg::Animated<float>> factor,
                bq::signal::AnySignal<avg::Animated<avg::Vector2f>> anchor =
                bq::signal::constant(avg::Vector2f(0.5f, 0.5f))) && // -> Shape
        {
            return detail::makeShapeUncheckedFromSignal(
                merge(std::move(func_), std::move(factor), std::move(anchor))
                .map([](auto func, avg::Animated<float> factor,
                        avg::Animated<avg::Vector2f> anchor)
                {
                    return makeShapeFunction(
                        [](avg::DrawContext const& drawContext,
                            avg::Vector2f size,
                            auto func,
                            float factor,
                            avg::Vector2f anchor) -> avg::Shape
                        {
                            avg::Vector2f offset(
                                    size.x() * anchor.x(),
                                    size.y() * anchor.y()
                                    );

                            return std::move(func(drawContext, size))
                                .translate(-offset)
                                .scale(factor)
                                .translate(offset)
                                ;
                        },
                        std::move(func),
                        std::move(factor),
                        std::move(anchor)
                        );
                }));
        }

        auto scale(float factor, avg::Vector2f anchor) && // -> Shape
        {
            return std::move(*this).scale(
                    bq::signal::constant(factor),
                    bq::signal::constant(anchor)
                    );
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
                bq::signal::Signal<T, avg::ShapeFunction> func)
        {
            return Shape<bq::signal::Signal<T, avg::ShapeFunction>>(
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
                bq::signal::SingleSignalTypeT<provider::ParamProviderTypeT<
                    std::decay_t<Ts>>>
            >>...
        >>>
    auto makeShape(TFunc&& func, Ts&&... ts)
    {
        return detail::makeShapeUnchecked(
                std::forward<TFunc>(func),
                std::forward<Ts>(ts)...
                );
    }
}
