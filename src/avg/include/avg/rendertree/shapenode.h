#pragma once

#include "rendertreenode.h"

#include "avg/animated.h"
#include "avg/drawing.h"
#include "avg/drawcontext.h"
#include "avg/shapefunction.h"

#include <btl/tuplereduce.h>
#include <btl/tupleforeach.h>

#include <optional>
#include <chrono>
#include <memory>
#include <tuple>
#include <typeindex>
#include <utility>

namespace avg
{
    template <typename... Ts>
    class ShapeNode : public RenderTreeNode
    {
    public:
        using DrawFunction = std::function<
            Drawing(DrawContext const&, Vector2f size,
                    AnimatedTypeT<Ts> const&...)
            >;

        ShapeNode(
                Animated<Obb> obb,
                std::optional<AnimationOptions> animationOptions,
                DrawFunction drawFunction,
                std::tuple<Ts...> data) :
            RenderTreeNode(
                    std::nullopt,
                    std::move(obb)
                    ),
            drawFunction_(std::move(drawFunction)),
            animationOptions_(std::move(animationOptions)),
            data_(std::move(data))
        {
        }

        ShapeNode(
                Animated<Obb> obb,
                std::optional<AnimationOptions> animationOptions,
                DrawFunction drawFunction,
                Ts&&... data
                ):
            RenderTreeNode(
                    std::nullopt,
                    std::move(obb)
                    ),
            drawFunction_(std::move(drawFunction)),
            animationOptions_(std::move(animationOptions)),
            data_(std::make_tuple(std::forward<Ts>(data)...))
        {
        }

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time) const final
        {
            bool cont = !hasAnimationEnded(data_, time);

            return std::make_pair(
                    std::apply([&, this](auto&&... ts)
                    {
                        return drawFunction_(
                                context,
                                getObbAt(time).getSize(),
                                getAnimatedValue(
                                    std::forward<decltype(ts)>(ts),
                                    time)...
                                )
                            .transform(obb.getTransform()
                                    * getObbAt(time).getTransform()
                                    );
                    },
                    data_
                    ),
                    cont
                    );
        }

        SnapshotNode snapshot(DrawContext const& context,
                avg::Obb const& parentObb,
                std::chrono::milliseconds time) const override
        {
            return makeLeafSnapshotNode("ShapeNode", *this, context, parentObb,
                    time);
        }

        UpdateResult update(
                RenderTree const& /*oldTree*/,
                RenderTree const& /*newTree*/,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override
        {
            if (oldNode && !newNode)
            {
                // Disappear
                return { nullptr, std::nullopt };
            }
            else if (!oldNode && newNode)
            {
                // Appear
                return { std::move(newNode), std::nullopt };
            }

            auto const& oldShape = reinterpret_cast<ShapeNode const&>(*oldNode);
            auto const& newShape = reinterpret_cast<ShapeNode const&>(*newNode);

            auto newAnimationOptions = newShape.animationOptions_
                ? newShape.animationOptions_
                : animationOptions
                ;

            return {
                std::make_shared<ShapeNode>(
                        oldNode->getObb().updated(newNode->getObb(),
                            newAnimationOptions, time),
                        newShape.animationOptions_,
                        newShape.drawFunction_,
                        getUpdatedAnimation(
                            oldShape.data_,
                            newShape.data_,
                            newAnimationOptions,
                            time
                            )
                        ),
                    std::nullopt
            };
        }

        std::optional<AnimationOptions> const& getAnimationOptions() const
        {
            return animationOptions_;
        }

        std::tuple<Ts...> const& getData() const
        {
            return data_;
        }

        std::type_index getType() const override
        {
            return typeid(ShapeNode);
        }

        std::shared_ptr<RenderTreeNode> clone() const override
        {
            return std::make_shared<ShapeNode>(
                    getObb(),
                    animationOptions_,
                    drawFunction_,
                    data_
                    );
        }

    private:
        DrawFunction drawFunction_;
        std::optional<AnimationOptions> animationOptions_;
        std::tuple<Ts...> data_;
    };

    template <typename... Ts>
    auto makeShapeNode(avg::Obb const& obb,
            std::optional<AnimationOptions> animationOptions,
            std::function<Drawing(DrawContext const&, Vector2f size,
                AnimatedTypeT<std::decay_t<Ts>> const&...)
            > function, Ts&&... ts)
    {
        return std::make_shared<ShapeNode<std::decay_t<Ts>...>>(
                obb, std::move(animationOptions), std::move(function),
                std::make_tuple(std::forward<Ts>(ts)...)
                );
    }
} // namespace avg
