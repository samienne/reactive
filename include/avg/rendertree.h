#pragma once

#include "btl/option.h"
#include "btl/tuplereduce.h"
#include "drawing.h"
#include "pmr/memory_resource.h"
#include "transform.h"
#include "avgvisibility.h"

#include <btl/tupleforeach.h>

#include <algorithm>
#include <chrono>
#include <type_traits>
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
    AVG_EXPORT Color lerp(Color const& a, Color const& b, float t);
    AVG_EXPORT Brush lerp(Brush const& a, Brush const& b, float t);
    AVG_EXPORT Pen lerp(Pen const& a, Pen const& b, float t);

    template <typename T>
    AVG_EXPORT btl::option<T> lerp(
            btl::option<T> const& a,
            btl::option<T> const& b,
            float t)
    {
        if (!a.valid())
            return b;
        if (!b.valid())
            return a;

        return btl::just(lerp(*a, *b, t));
    }

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
        template <typename U, typename = std::enable_if_t<
            std::is_convertible_v<U, T>
            >>
        Animated(U&& value) :
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

        bool hasAnimationEnded(std::chrono::milliseconds time) const
        {
            return (time - beginTime_) > duration_;
        }

        Animated updated(
                Animated const& newValue,
                AnimationOptions const& options,
                std::chrono::milliseconds time
                ) const
        {
            if (hasAnimationEnded(time)
                    && getFinalValue() == newValue.getFinalValue())
            {
                return *this;

            }

            return Animated(
                    getValue(time),
                    newValue.getFinalValue(),
                    options.curve,
                    time,
                    options.duration
                    );
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
        Obb getObbAt(std::chrono::milliseconds time) const;
        Animated<Obb> const& getObb() const;
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

        virtual std::pair<Drawing, bool> draw(pmr::memory_resource* memory,
                std::chrono::milliseconds time
                ) const = 0;

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

        std::pair<Drawing, bool> draw(pmr::memory_resource* memory,
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

        std::pair<Drawing, bool> draw(pmr::memory_resource* memory,
                std::chrono::milliseconds time) const final
        {
            bool cont = btl::tuple_reduce(false, data_,
                    [&](bool cont, auto const& data)
                    {
                        return cont || !data.hasAnimationEnded(time);
                    });

            return std::make_pair(
                    std::apply([&, this](auto&&... ts)
                    {
                        return drawFunction_(
                                memory,
                                getObbAt(time).getSize(),
                                std::forward<decltype(ts)>(ts).getValue(time)...
                                )
                            .transform(getObbAt(time).getTransform());
                    },
                    data_
                    ),
                    cont
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
                    oldNode->getObb().updated(newNode->getObb(),
                        animationOptions, time),
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
                        std::get<S>(a).updated(
                            std::get<S>(b), animationOptions, time)
                        )...
                    );
        }

    private:
        DrawFunction drawFunction_;
        std::tuple<Animated<Ts>...> data_;
    };

    class AVG_EXPORT RectNode : public ShapeNode<float,
        btl::option<Brush>, btl::option<Pen>>
    {
    public:
        RectNode(
                UniqueId id,
                Animated<Obb> obb,
                TransitionOptions transitionOptions,
                Animated<float> radius,
                Animated<btl::option<Brush>> brush,
                Animated<btl::option<Pen>> pen
                );

    private:
        static Drawing drawRect(
                pmr::memory_resource* memory,
                Vector2f size,
                float radius,
                btl::option<Brush> const& brush,
                btl::option<Pen> const& pen
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

        std::pair<Drawing, bool> draw(pmr::memory_resource* memory,
                std::chrono::milliseconds time) const;

        //RenderTreeNode const* findNode(UniqueId id) const;

    private:
        //std::unordered_map<UniqueId, RenderTreeNode*> nodes_;
        std::shared_ptr<RenderTreeNode> root_;
    };
} // namespace avg

