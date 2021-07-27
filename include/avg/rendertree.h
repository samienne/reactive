#pragma once

#include "drawing.h"
#include "pmr/memory_resource.h"
#include "transform.h"
#include "avgvisibility.h"

#include <btl/tupleforeach.h>

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <utility>

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

    private:
        uint64_t value_;
        static std::atomic<uint64_t> nextValue_;
    };

    struct AVG_EXPORT AnimationOptions
    {
        std::chrono::milliseconds duration;
        std::function<float(float)> curve;
    };

    struct AVG_EXPORT TransitionOptions
    {
    };

    AVG_EXPORT float linearCurve(float f);

    AVG_EXPORT float lerp(float a, float b, float t);
    AVG_EXPORT Vector2f lerp(Vector2f a, Vector2f b, float t);
    AVG_EXPORT Rect lerp(Rect a, Rect b, float t);
    AVG_EXPORT Transform lerp(Transform const a, Transform const& b, float t);
    AVG_EXPORT Obb lerp(Obb const& a, Obb const& b, float t);

    template <typename... Ts, size_t... S>
    std::tuple<Ts...> tupleLerp(
            std::tuple<Ts...> const& a,
            std::tuple<Ts...> const& b,
            float t,
            std::index_sequence<S...>)
    {
        return std::make_tuple(
                lerp(std::get<S>(a), std::get<S>(b), t)...
                );
    }

    template <typename... Ts>
    std::tuple<Ts...> lerp(std::tuple<Ts...> const& a,
            std::tuple<Ts...> const& b, float t)
    {
        return tuple_lerp(a, b, t, std::make_index_sequence<sizeof...(Ts)>());
    }

    template <typename T>
    class Animated
    {
    public:
        Animated(T value) :
            initial_(value),
            final_(value),
            curve_(linearCurve),
            beginTime_(std::chrono::milliseconds(0)),
            duration_(std::chrono::milliseconds(0))
        {
        }

        Animated(T initialValue, T finalValue,
                std::function<float(float)> curve,
                std::chrono::milliseconds beginTime,
                std::chrono::milliseconds duration
                ) :
            initial_(std::move(initialValue)),
            final_(std::move(finalValue)),
            curve_(std::move(curve)),
            beginTime_(beginTime),
            duration_(duration)
        {
        }

        T getValue(std::chrono::milliseconds time) const
        {
            if (duration_ <= std::chrono::milliseconds(0))
                return final_;

            float a = std::clamp(
                    (float)(time - beginTime_).count() / (float)duration_.count(),
                    0.0f,
                    1.0f
                    );

            return lerp(initial_, final_, a);
        }

        T const& getFinalValue() const
        {
            return final_;
        }

        std::chrono::milliseconds getDuration() const
        {
            return duration_;
        }

    private:
        T initial_;
        T final_;
        std::function<float(float)> curve_;
        std::chrono::milliseconds beginTime_;
        std::chrono::milliseconds duration_;
    };

    class AVG_EXPORT RenderTree;

    class AVG_EXPORT RenderTreeNode
    {
    public:
        RenderTreeNode(
                UniqueId id,
                Animated<Obb> obb,
                TransitionOptions const& transition
                );

        virtual ~RenderTreeNode() = default;

        UniqueId const& getId() const;
        Obb getObb(std::chrono::milliseconds time) const;
        Obb getFinalObb() const;
        TransitionOptions const& getTransitionOptions() const;

        virtual std::shared_ptr<RenderTreeNode> update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                AnimationOptions const& animationOptions,
                std::chrono::milliseconds time
                ) const = 0;

        virtual Drawing draw(pmr::memory_resource* memory,
                std::chrono::milliseconds time) const = 0;

    private:
        UniqueId id_;
        UniqueId geometryId_;
        TransitionOptions transitionOptions_;
        Animated<Obb> obb_;
    };

    class AVG_EXPORT ContainerNode : public RenderTreeNode
    {
    public:
        ContainerNode(
                UniqueId id,
                Animated<Obb> obb,
                TransitionOptions transitionOptions,
                std::vector<std::shared_ptr<RenderTreeNode>> children
                );

        std::shared_ptr<RenderTreeNode> update(
                RenderTree const& oldTree,
                RenderTree const& newTree,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                AnimationOptions const& animationOptions,
                std::chrono::milliseconds time
                ) const override;

        Drawing draw(pmr::memory_resource* memory,
                std::chrono::milliseconds time) const override;

    private:
        std::vector<std::shared_ptr<RenderTreeNode>> children_;
    };

    template <typename... Ts>
    class ShapeNode : public RenderTreeNode
    {
    public:
        using DrawFunction = std::function<
            Drawing(pmr::memory_resource*, Vector2f size, Ts const&...)
            >;

        ShapeNode(UniqueId id,
                Animated<Obb> obb,
                TransitionOptions transitionOptions,
                DrawFunction drawFunction,
                std::tuple<Animated<Ts>...> data) :
            RenderTreeNode(
                    id,
                    std::move(obb),
                    std::move(transitionOptions)
                    ),
            drawFunction_(std::move(drawFunction)),
            data_(std::move(data))
        {
        }

        ShapeNode(UniqueId id,
                Animated<Obb> obb,
                TransitionOptions transitionOptions,
                DrawFunction drawFunction,
                Animated<Ts>... data
                ):
            RenderTreeNode(
                    id,
                    std::move(obb),
                    std::move(transitionOptions)
                    ),
            drawFunction_(std::move(drawFunction)),
            data_(std::make_tuple(std::move(data)...))
        {
        }

        Drawing draw(pmr::memory_resource* memory,
                std::chrono::milliseconds time) const final
        {
            return std::apply([&, this](auto&&... ts)
                    {
                        return drawFunction_(
                                memory,
                                getObb(time).getSize(),
                                std::forward<decltype(ts)>(ts).getValue(time)...
                                );
                    },
                    data_
                    );
        }

        std::shared_ptr<RenderTreeNode> update(
                RenderTree const& /*oldTree*/,
                RenderTree const& /*newTree*/,
                std::shared_ptr<RenderTreeNode> const& oldNode,
                std::shared_ptr<RenderTreeNode> const& newNode,
                AnimationOptions const& animationOptions,
                std::chrono::milliseconds time
                ) const override
        {
            if (oldNode && !newNode)
            {
                // Disappear
                return nullptr;
            }
            else if (!oldNode && newNode)
            {
                // Appear
                return newNode;
            }

            auto const& oldShape = reinterpret_cast<ShapeNode const&>(*oldNode);
            auto const& newShape = reinterpret_cast<ShapeNode const&>(*newNode);

            return std::make_shared<ShapeNode>(
                    newNode->getId(),
                    Animated<Obb>(
                        oldNode->getObb(time),
                        newNode->getFinalObb(),
                        animationOptions.curve,
                        time,
                        animationOptions.duration
                        ),
                    newNode->getTransitionOptions(),
                    newShape.drawFunction_,
                    updateTuples(
                        oldShape.data_,
                        newShape.data_,
                        time,
                        animationOptions,
                        std::make_index_sequence<sizeof...(Ts)>()
                        )
                    );
        }

    private:
        template <size_t... S>
        static std::tuple<Animated<Ts>...> updateTuples(
                std::tuple<Animated<Ts>...> const& a,
                std::tuple<Animated<Ts>...> const& b,
                std::chrono::milliseconds time,
                AnimationOptions const& animationOptions,
                std::index_sequence<S...>
                )
        {
            return std::make_tuple(
                    std::tuple_element_t<S, std::tuple<Animated<Ts>...>>(
                        std::get<S>(a).getValue(time),
                        std::get<S>(b).getFinalValue(),
                        animationOptions.curve,
                        time,
                        animationOptions.duration
                        )...
                    );
        }

    private:
        DrawFunction drawFunction_;
        std::tuple<Animated<Ts>...> data_;
    };

    class AVG_EXPORT RectNode : public ShapeNode<float>
    {
    public:
        RectNode(
                UniqueId id,
                Animated<Obb> obb,
                TransitionOptions transitionOptions,
                Animated<float> radius
                );

    private:
        static Drawing drawRect(
                pmr::memory_resource* memory,
                Vector2f size,
                float radius
                );
    };

    class AVG_EXPORT RenderTree
    {
    public:
        RenderTree();
        RenderTree(std::shared_ptr<RenderTreeNode> root);

        RenderTree update(
                RenderTree&& tree,
                AnimationOptions const& animationOptions,
                std::chrono::milliseconds time
                ) &&;

        Drawing draw(pmr::memory_resource* memory,
                std::chrono::milliseconds time) const;

        //RenderTreeNode const* findNode(UniqueId id) const;

    private:
        //std::unordered_map<UniqueId, RenderTreeNode*> nodes_;
        std::shared_ptr<RenderTreeNode> root_;
    };
} // namespace avg

