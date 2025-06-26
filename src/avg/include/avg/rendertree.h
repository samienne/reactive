#pragma once

#include "animated.h"
#include "drawing.h"
#include "transform.h"
#include "drawcontext.h"
#include "shapefunction.h"
#include "avgvisibility.h"

#include <pmr/memory_resource.h>

#include <btl/tuplereduce.h>
#include <btl/tupleforeach.h>

#include <optional>
#include <algorithm>
#include <chrono>
#include <type_traits>
#include <typeindex>
#include <atomic>
#include <utility>
#include <iostream>

namespace avg
{
    class AVG_EXPORT UniqueId
    {
    public:
        UniqueId();
        UniqueId(UniqueId const&) = default;

        UniqueId& operator=(UniqueId const&) = default;

        bool operator==(UniqueId const& id) const;
        bool operator!=(UniqueId const& id) const;
        bool operator<(UniqueId const& id) const;
        bool operator>(UniqueId const& id) const;

        AVG_EXPORT friend std::ostream& operator<<(std::ostream& stream,
                UniqueId const& id);

    private:
        uint64_t value_;
        static std::atomic<uint64_t> nextValue_;
    };

    class AVG_EXPORT RenderTree;
    class RenderTreeNode;

    struct AVG_EXPORT UpdateResult
    {
        std::shared_ptr<RenderTreeNode> node;
        std::optional<std::chrono::milliseconds> nextUpdate;
    };

    AVG_EXPORT std::optional<std::chrono::milliseconds> earlier(
            std::optional<std::chrono::milliseconds> t1,
            std::optional<std::chrono::milliseconds> t2
            );

    class AVG_EXPORT RenderTreeNode
    {
    public:
        RenderTreeNode(
                std::optional<UniqueId> id,
                Animated<Obb> obb
                );

        virtual ~RenderTreeNode() = default;

        std::optional<UniqueId> const& getId() const;
        Obb getObbAt(std::chrono::milliseconds time) const;
        Animated<Obb> const& getObb() const;
        Obb getFinalObb() const;

        void transform(Transform const& transform);

        virtual UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const = 0;

        virtual std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time
                ) const = 0;

        virtual std::type_index getType() const = 0;
        virtual std::shared_ptr<RenderTreeNode> clone() const = 0;

    private:
        std::optional<UniqueId> id_;
        Animated<Obb> obb_;
    };

    class AVG_EXPORT ContainerNode : public RenderTreeNode
    {
        struct Child
        {
            inline Child(
                std::shared_ptr<RenderTreeNode> node,
                bool active
                ) :
                node(std::move(node)),
                active(active)
            {
            }

            std::shared_ptr<RenderTreeNode> node;
            bool active = false;
        };

    public:
        ContainerNode(
                Animated<Obb> obb,
                std::vector<Child> children = {}
                );

        ContainerNode(ContainerNode const&) = default;

        void addChild(std::shared_ptr<RenderTreeNode> node);
        void addChildBehind(std::shared_ptr<RenderTreeNode> node);

        UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& parentObb,
                std::chrono::milliseconds time) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        std::vector<Child> children_;
    };

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

    class AVG_EXPORT RectNode : public ShapeNode<Animated<float>,
        Animated<std::optional<Brush>>, Animated<std::optional<Pen>>>
    {
    public:
        RectNode(
                Animated<Obb> obb,
                std::optional<AnimationOptions> animationOptions,
                Animated<float> radius,
                Animated<std::optional<Brush>> brush,
                Animated<std::optional<Pen>> pen
                );

        std::shared_ptr<RenderTreeNode> clone() const override
        {
            return std::make_shared<RectNode>(
                    getObb(),
                    getAnimationOptions(),
                    std::get<0>(getData()),
                    std::get<1>(getData()),
                    std::get<2>(getData())
                    );
        }

    private:
        static Drawing drawRect(
                DrawContext const& drawContext,
                Vector2f size,
                float radius,
                std::optional<Brush> const& brush,
                std::optional<Pen> const& pen
                );
    };

    class AVG_EXPORT TransitionNode : public RenderTreeNode
    {
    public:
        enum class State
        {
            transitioned,
            transitioning,
            active,
            activating
        };

        struct Transition
        {
            std::chrono::milliseconds startTime;
            std::chrono::milliseconds duration;
        };

        TransitionNode(
                Animated<Obb> obb,
                bool isActive,
                std::shared_ptr<RenderTreeNode> activeNode,
                std::shared_ptr<RenderTreeNode> transitionedNode
                );

        TransitionNode(TransitionNode const&) = default;

        UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time
                ) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        std::shared_ptr<RenderTreeNode> activeNode_;
        std::shared_ptr<RenderTreeNode> transitionedNode_;
        std::optional<Transition> transition_;
        bool isActive_ = true;
    };

    class AVG_EXPORT ClipNode : public RenderTreeNode
    {
    public:
        ClipNode(
                Animated<Obb> obb,
                std::shared_ptr<RenderTreeNode> childNode
                );

        ClipNode(ClipNode const&) = default;

        UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time
                ) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        std::shared_ptr<RenderTreeNode> childNode_;
    };

    class AVG_EXPORT IdNode : public RenderTreeNode
    {
    public:
        IdNode(
                UniqueId id,
                Animated<Obb> obb,
                std::shared_ptr<RenderTreeNode> childNode
                );

        IdNode(IdNode const&) = default;

        UpdateResult update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time
                ) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        std::shared_ptr<RenderTreeNode> childNode_;
    };

    class AVG_EXPORT RenderTree
    {
    public:
        RenderTree();
        RenderTree(std::shared_ptr<RenderTreeNode> root);

        RenderTree(RenderTree const&) = default;
        RenderTree(RenderTree&&) noexcept = default;

        RenderTree& operator=(RenderTree const&) = default;
        RenderTree& operator=(RenderTree&&) noexcept = default;

        std::pair<RenderTree, std::optional<std::chrono::milliseconds>> update(
                RenderTree&& tree,
                std::optional<AnimationOptions> const& animationOptions,
                std::chrono::milliseconds time
                ) &&;

        std::pair<Drawing, bool> draw(DrawContext const& drawContext,
                avg::Obb const& obb,
                std::chrono::milliseconds time) const;

        RenderTree transform(Transform const& transform) &&;

        std::shared_ptr<RenderTreeNode> const& getRoot() const;

    private:
        std::shared_ptr<RenderTreeNode> root_;
    };
} // namespace avg

