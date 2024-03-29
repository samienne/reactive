#pragma once

#include <reactive/widget/ondraw.h>
#include <reactive/widget/widget.h>

#include <reactive/signal/flipoptional.h>

#include <avg/animated.h>
#include <avg/brush.h>
#include <avg/drawcontext.h>
#include <avg/drawnode.h>
#include <avg/path.h>
#include <avg/pen.h>
#include <avg/shape.h>
#include <avg/shapefunction.h>
#include <avg/vector.h>

#include <btl/bindarguments.h>

#include <type_traits>

namespace reactive::shape
{
    struct ShapeBuildTag {};

    namespace detail
    {
        template <typename T, typename TFunc>
        auto makeShapeUnchecked(Signal<T, TFunc>&& func);
    }

    template <typename T>
    class Shape;

    using AnyShape = Shape<AnySignal<avg::ShapeFunction>>;

    template <typename T, typename U, typename V>
    auto makeDrawModifier(
            Signal<T, avg::ShapeFunction> func,
            Signal<U, std::optional<avg::Brush>> brush,
            Signal<V, std::optional<avg::Pen>> pen
            )
    {
        return widget::makeWidgetModifier([](auto widget, auto animationOptions,
                    auto func, auto brush, auto pen)
            {
                return std::move(widget)
                | widget::makeInstanceModifier(
                    [](widget::Instance instance,
                        std::optional<avg::AnimationOptions> const& animationOptions,
                        avg::ShapeFunction const& func,
                        std::optional<avg::Brush> const& brush,
                        std::optional<avg::Pen> const& pen) -> widget::Instance
                    {
                        auto node = std::make_shared<avg::DrawNode>(
                                instance.getObb(),
                                func,
                                brush,
                                pen,
                                animationOptions
                                );

                        auto container = std::make_shared<avg::ContainerNode>(avg::Obb());

                        bool reverse = false;
                        if (reverse)
                        {
                            container->addChild(std::move(node));
                            container->addChild(instance.getRenderTree().getRoot());
                        }
                        else
                        {
                            container->addChild(instance.getRenderTree().getRoot());
                            container->addChild(std::move(node));
                        }

                        return std::move(instance)
                            .setRenderTree(avg::RenderTree(std::move(container)))
                            ;
                    },
                    std::move(animationOptions),
                    std::move(func),
                    std::move(brush),
                    std::move(pen)
                    );
            },
            widget::provideAnimation(),
            std::move(func),
            std::move(brush),
            std::move(pen)
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
        auto stroke(Signal<U, avg::Pen> pen) && // -> Widget
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
        auto fill(Signal<U, avg::Brush> brush) && // -> Widget
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
        auto strokeToShape(Signal<T, avg::Pen> pen) && // -> Shape
        {
        }

        widget::AnyWidget fillAndStroke(std::optional<AnySignal<avg::Brush>> brush,
            std::optional<AnySignal<avg::Pen>> pen) && // -> Widget
        {
            return widget::makeWidget()
                | makeDrawModifier(
                        func_.clone(),
                        signal::flipOptional(std::move(brush)),
                        signal::flipOptional(std::move(pen)))
                ;
        }

        auto clip(Shape clipShape) // -> Shape
        {
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
        auto makeShapeUncheckedFromSignal(Signal<T, avg::ShapeFunction> func)
        {
            return Shape<Signal<T, avg::ShapeFunction>>(
                    ShapeBuildTag{},
                    std::move(func));
        }

        template <typename TFunc, typename... Ts>
        auto makeShapeUnchecked(TFunc&& func, Ts&&... ts)
        {
            return makeShapeUncheckedFromSignal(
                signal::group(std::forward<Ts>(ts)...).map(
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
                signal::SignalType<widget::ParamProviderTypeT<std::decay_t<Ts>>>
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
