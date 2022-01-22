#pragma once

#include "drawing.h"
#include "transform.h"
#include "drawcontext.h"
#include "avgvisibility.h"

#include <pmr/memory_resource.h>

#include <btl/option.h>
#include <btl/tuplereduce.h>
#include <btl/tupleforeach.h>

#include <optional>
#include <algorithm>
#include <chrono>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
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


    struct AVG_EXPORT AnimationOptions
    {
        std::chrono::milliseconds duration;
        std::function<float(float)> curve;
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
    btl::option<T> lerp(
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
    using LerpType = decltype(
            lerp(
                std::declval<std::decay_t<T>>(),
                std::declval<std::decay_t<T>>(),
                0.0f
                )
            );

    template <typename T, typename = void>
    struct HasLerp : std::false_type {};

    template <typename T>
    struct HasLerp<T, btl::void_t<
        LerpType<T>
        >> : std::true_type {};

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
            if (getDuration() <= std::chrono::milliseconds(0))
            {
                return final_;
            }

            float a = std::clamp(
                    (float)(time - beginTime_).count() / (float)getDuration().count(),
                    0.0f,
                    1.0f
                    );

            if constexpr(HasLerp<T>::value)
                return lerp(initial_, final_, a);
            else
                return a <= 0.0f ? initial_ : final_;
        }

        T const& getInitialValue() const
        {
            return initial_;
        }

        T const& getFinalValue() const
        {
            return final_;
        }

        std::function<float(float)> const& getCurve() const
        {
            return curve_;
        };

        std::chrono::milliseconds getBeginTime() const
        {
            return beginTime_;
        }

        std::chrono::milliseconds getDuration() const
        {
            if constexpr(HasLerp<T>::value)
                return duration_;
            else
                return std::chrono::milliseconds(0);
        }

        bool hasAnimationEnded(std::chrono::milliseconds time) const
        {
            return time >= (beginTime_ + getDuration());
        }

        bool isAnimationRunning(std::chrono::milliseconds time) const
        {
            return beginTime_ < time && time < (beginTime_ + getDuration());
        }

        Animated updated(
                Animated const& newValue,
                std::optional<AnimationOptions> const& options,
                std::chrono::milliseconds time
                ) const
        {
            if (getFinalValue() == newValue.getFinalValue())
                return *this;

            if (!options)
            {
                if (isAnimationRunning(time))
                    return *this;
                else
                    return newValue;
            }

            return Animated(
                    getValue(time),
                    newValue.getFinalValue(),
                    options->curve,
                    time,
                    options->duration
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
                avg::Obb const& obb,
                std::chrono::milliseconds time) const override;

        std::type_index getType() const override;
        std::shared_ptr<RenderTreeNode> clone() const override;

    private:
        std::vector<Child> children_;
    };

    template <typename T>
    struct IsAnimated : std::false_type {};

    template <typename T>
    struct IsAnimated<Animated<T>> : std::true_type {};

    template <typename T>
    struct AnimatedType
    {
        using type = std::remove_reference_t<std::remove_cv_t<T>>;
    };

    template <typename T>
    struct AnimatedType<Animated<T>>
    {
        using type = std::remove_reference_t<std::remove_cv_t<T>>;
    };

    template <typename T>
    using AnimatedTypeT = typename AnimatedType<T>::type;

    template <typename... Ts>
    class ShapeNode : public RenderTreeNode
    {
    public:
        using DrawFunction = std::function<
            Drawing(DrawContext const&, Vector2f size,
                    typename AnimatedType<Ts>::type const&...)
            >;

        ShapeNode(
                Animated<Obb> obb,
                DrawFunction drawFunction,
                std::tuple<Ts...> data) :
            RenderTreeNode(
                    std::nullopt,
                    std::move(obb)
                    ),
            drawFunction_(std::move(drawFunction)),
            data_(std::move(data))
        {
        }

        ShapeNode(
                Animated<Obb> obb,
                DrawFunction drawFunction,
                Ts&&... data
                ):
            RenderTreeNode(
                    std::nullopt,
                    std::move(obb)
                    ),
            drawFunction_(std::move(drawFunction)),
            data_(std::make_tuple(std::forward<Ts>(data)...))
        {
        }

        std::pair<Drawing, bool> draw(DrawContext const& context,
                avg::Obb const& obb,
                std::chrono::milliseconds time) const final
        {
            bool cont = btl::tuple_reduce(false, data_,
                    [&](bool cont, auto const& data)
                    {
                        if constexpr (IsAnimated<std::decay_t<decltype(data)>>::value)
                        {
                            return cont || !data.hasAnimationEnded(time);
                        }
                        else
                        {
                            return cont;
                        }
                    });

            return std::make_pair(
                    std::apply([&, this](auto&&... ts)
                    {
                        return drawFunction_(
                                context,
                                getObbAt(time).getSize(),
                                [&](auto&& t) -> decltype(auto)
                                {
                                    if constexpr (IsAnimated<
                                            std::decay_t<decltype(t)>
                                            >::value)
                                    {
                                        return std::forward<decltype(t)>(t).getValue(time);
                                    }
                                    else
                                    {
                                        return std::forward<decltype(t)>(t);
                                    }
                                }(
                                    std::forward<decltype(ts)>(ts)
                                )...
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

            return {
                std::make_shared<ShapeNode>(
                        oldNode->getObb().updated(newNode->getObb(),
                            animationOptions, time),
                        newShape.drawFunction_,
                        updateTuples(
                            oldShape.data_,
                            newShape.data_,
                            time,
                            animationOptions,
                            std::make_index_sequence<sizeof...(Ts)>()
                            )
                        ),
                    std::nullopt
            };
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
                    drawFunction_,
                    data_
                    );
        }

    private:
        template <typename T>
        static auto updateIfAnimated(
                T const&,
                T const& b,
                std::chrono::milliseconds,
                std::optional<AnimationOptions> const&)
        {
            return b;
        }

        template <typename T>
        static auto updateIfAnimated(
                Animated<T> const& a,
                Animated<T> const& b,
                std::chrono::milliseconds time,
                std::optional<AnimationOptions> const& animationOptions)
        {
            return a.updated(b, animationOptions, time);
        }

        template <size_t... S>
        static std::tuple<Ts...> updateTuples(
                std::tuple<Ts...> const& a,
                std::tuple<Ts...> const& b,
                std::chrono::milliseconds time,
                std::optional<AnimationOptions> const& animationOptions,
                std::index_sequence<S...>
                )
        {
            return std::make_tuple(
                    std::tuple_element_t<S, std::tuple<Ts...>>(
                        /*
                        std::get<S>(a).updated(
                            std::get<S>(b), animationOptions, time)
                        */
                        updateIfAnimated(std::get<S>(a), std::get<S>(b),
                            time, animationOptions)
                        )...
                    );
        }

    private:
        DrawFunction drawFunction_;
        std::tuple<Ts...> data_;
    };

    template <typename... Ts>
    auto makeShapeNode(avg::Obb const& obb,
            std::function<
            Drawing(DrawContext const&, Vector2f size, AnimatedTypeT<Ts> const&...)
            > function, Ts&&... ts)
    {
        return std::make_shared<ShapeNode<std::decay_t<Ts>...>>(
                obb, std::move(function),
                std::make_tuple(std::forward<Ts>(ts)...)
                );
    }

    class AVG_EXPORT RectNode : public ShapeNode<Animated<float>,
        Animated<btl::option<Brush>>, Animated<btl::option<Pen>>>
    {
    public:
        RectNode(
                Animated<Obb> obb,
                Animated<float> radius,
                Animated<btl::option<Brush>> brush,
                Animated<btl::option<Pen>> pen
                );

        std::shared_ptr<RenderTreeNode> clone() const override
        {
            return std::make_shared<RectNode>(
                    getObb(),
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
                btl::option<Brush> const& brush,
                btl::option<Pen> const& pen
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

