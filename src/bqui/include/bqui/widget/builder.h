#pragma once

#include "element.h"

#include "bqui/provider/paramprovider.h"

#include "bqui/buildparams.h"
#include "bqui/widget/boxvariables.h"
#include "bqui/widget/layoutspec.h"

#include <bq/signal/signal.h>

#include <avg/transform.h>

#include <btl/cloneoncopy.h>

#include <functional>
#include <type_traits>

namespace bqui::widget
{
    template <typename TFunc>
    class Builder;

    struct AnyBuilder;

    using BuilderBase = Builder<
        std::function<widget::AnyElement(BuildParams params,
                bq::signal::AnySignal<avg::Vector2f>,
                bq::signal::AnySignal<LayoutSolution>)>
        >;

    template <typename T>
    using IsBuilder = typename std::is_convertible<T, AnyBuilder>::type;

    namespace detail
    {
        // A build function is either the ordinary two-argument shape
        // (BuildParams, size) or, for a pure-solver container that must place
        // its children from the region solution, the three-argument shape with
        // the solution appended. invokeBuild() calls whichever it is, so a leaf
        // that ignores the solution needs no editing. The two-argument shape is
        // tested first: a bindArguments-bound build accepts extra trailing
        // arguments, so only ruling out the two-argument call keeps the solution
        // from colliding with a bound argument.
        template <typename TFunc, typename TSize>
        auto invokeBuild(TFunc& func, BuildParams params, TSize size,
                bq::signal::AnySignal<LayoutSolution> solution)
        {
            if constexpr (std::is_invocable_v<TFunc&, BuildParams, TSize>)
                return std::invoke(func, std::move(params), std::move(size));
            else
                return std::invoke(func, std::move(params), std::move(size),
                        std::move(solution));
        }

        // Widens any build function to the three-argument shape the type-erased
        // BuilderBase stores, dropping the solution for the two-argument ones.
        template <typename TFunc>
        auto adaptBuild(TFunc func)
        {
            return [func = std::move(func)](BuildParams params,
                    bq::signal::AnySignal<avg::Vector2f> size,
                    bq::signal::AnySignal<LayoutSolution> solution) mutable
                    -> widget::AnyElement
            {
                return invokeBuild(func, std::move(params), std::move(size),
                        std::move(solution));
            };
        }

        // Whether TFunc is a build function of either shape. Tested lazily: a
        // build that takes the solution is only accepted when it does not also
        // take the two-argument call, and -- crucially -- the three-argument test
        // is never instantiated for a build that does, since a bindArguments-bound
        // build would hard-error deducing the return type of a solution
        // mis-bound as one of its arguments.
        template <typename TFunc>
        using IsBuildFunc = std::disjunction<
            std::is_invocable<TFunc, BuildParams,
                bq::signal::AnySignal<avg::Vector2f>>,
            std::conjunction<
                std::negation<std::is_invocable<TFunc, BuildParams,
                    bq::signal::AnySignal<avg::Vector2f>>>,
                std::is_invocable<TFunc, BuildParams,
                    bq::signal::AnySignal<avg::Vector2f>,
                    bq::signal::AnySignal<LayoutSolution>>>>;
    } // namespace detail

    template <typename TFunc, typename = std::enable_if_t<
        detail::IsBuildFunc<TFunc>::value
        >
    >
    auto makeBuilder(TFunc&& func, BuildParams params,
            bq::signal::AnySignal<avg::Vector2f> gravity)
    {
        return Builder<std::decay_t<TFunc>>(
                std::forward<TFunc>(func),
                std::move(params),
                std::move(gravity)
                );
    }

    template <typename TFunc>
    class Builder
    {
    public:
        Builder(TFunc func, BuildParams params,
                bq::signal::AnySignal<avg::Vector2f> gravity) :
            func_(std::move(func)),
            buildParams_(std::move(params)),
            gravity_(std::move(gravity))
        {
        }

        Builder clone() const
        {
            return *this;
        }

        Builder(Builder const&) = default;
        Builder& operator=(Builder const&) = default;

        Builder(Builder&&) noexcept = default;
        Builder& operator=(Builder&&) noexcept = default;

        template <typename T>
        auto operator()(bq::signal::Signal<T, avg::Vector2f> size) &&
        {
            return std::move(*this)(std::move(size),
                    bq::signal::AnySignal<LayoutSolution>(
                        bq::signal::constant(LayoutSolution())));
        }

        template <typename T>
        auto operator()(bq::signal::Signal<T, avg::Vector2f> size,
                bq::signal::AnySignal<LayoutSolution> solution) &&
        {
            return detail::invokeBuild(*func_, std::move(buildParams_),
                    std::move(size), std::move(solution));
        }

        /**
         * @brief This widget's accumulated pure-solver constraints, composed up
         * from its children; empty on a builder minted without one. Preserved
         * across a copy and type erasure, exactly as the box variables
         * are.
         */
        PureLayout const& getPureLayout() const
        {
            return pureLayout_;
        }

        /**
         * @brief Replaces this widget's composed pure-solver constraints, used to
         * carry them across a rebuild that mints a fresh builder.
         */
        void setPureLayout(PureLayout pureLayout)
        {
            pureLayout_ = std::move(pureLayout);
        }

        /**
         * @brief The four edge variables that name this widget's box to the
         * constraint solver. Stable for the builder's lifetime and preserved
         * across a copy and type erasure.
         */
        BoxVariables const& getBoxVariables() const
        {
            return box_;
        }

        /**
         * @brief Adopts @p box as this widget's solver box, so a transformed
         * builder keeps the identity of the one it came from.
         */
        void setBoxVariables(BoxVariables box)
        {
            box_ = std::move(box);
        }

        auto setBuildParams(BuildParams params) &&
        {
            auto builder = makeBuilder([params=std::move(buildParams_),
                    func=std::move(func_)](BuildParams oldParams,
                        bq::signal::AnySignal<avg::Vector2f> size,
                        bq::signal::AnySignal<LayoutSolution> solution)
                {
                    return detail::invokeBuild(*func, params, std::move(size),
                            std::move(solution)).setParams(oldParams);
                },
                std::move(params),
                std::move(gravity_)
                );
            builder.setBoxVariables(box_);
            builder.setPureLayout(pureLayout_);
            builder.setGravityExplicit(gravityExplicit_);
            return builder;
        }

        BuildParams const& getBuildParams() const
        {
            return buildParams_;
        }

        auto setGravity(bq::signal::AnySignal<avg::Vector2f> gravity)
        {
            auto copy = clone();
            copy.gravity_ = std::move(gravity);
            copy.gravityExplicit_ = true;
            return copy;
        }

        bq::signal::AnySignal<avg::Vector2f> getGravity() const
        {
            return gravity_;
        }

        /**
         * @brief Whether the gravity was stated by setGravity() rather than left
         * at the default.
         *
         * A pure box places a child across its axis by gravity only when it is
         * stated, and otherwise keeps it at the leading edge. Preserved across a
         * copy and type erasure, exactly as the gravity is.
         */
        bool isGravityExplicit() const
        {
            return gravityExplicit_;
        }

        /**
         * @brief Carries isGravityExplicit() onto a builder minted afresh from
         * this one's gravity.
         */
        void setGravityExplicit(bool value)
        {
            gravityExplicit_ = value;
        }

        operator BuilderBase() &&
        {
            BuilderBase base(
                    detail::adaptBuild(std::move(*func_)),
                    std::move(buildParams_),
                    std::move(gravity_)
                    );
            base.setBoxVariables(box_);
            base.setPureLayout(pureLayout_);
            base.setGravityExplicit(gravityExplicit_);
            return base;
        }

    protected:
        btl::CloneOnCopy<TFunc> func_;
        BuildParams buildParams_;
        bq::signal::AnySignal<avg::Vector2f> gravity_ =
            bq::signal::constant(avg::Vector2f(0.5f, 0.5f));
        bool gravityExplicit_ = false;
        BoxVariables box_;
        PureLayout pureLayout_ = emptyPureLayout();
    };

    struct AnyBuilder : Builder<std::function<widget::AnyElement(
            BuildParams, bq::signal::AnySignal<avg::Vector2f>,
            bq::signal::AnySignal<LayoutSolution>)>>
    {
        AnyBuilder(AnyBuilder const&) = default;
        AnyBuilder(AnyBuilder&&) noexcept = default;

        AnyBuilder& operator=(AnyBuilder const&) = default;
        AnyBuilder& operator=(AnyBuilder&&) noexcept = default;

        template <typename TFunc>
        AnyBuilder(Builder<TFunc> base) :
            BuilderBase(std::move(base))
        {
        }

        AnyBuilder clone() const
        {
            return *this;
        }
    };

    template <typename TFunc, typename T, typename... Ts, typename = std::enable_if_t<
        std::is_invocable_r_v<AnyBuilder, TFunc, bq::signal::AnySignal<avg::Vector2f>,
        provider::ParamProviderTypeT<Ts>...>
    >>
    auto makeBuilderWithSize(TFunc&& func, Ts&&... ts)
    {
        return makeBuilder(btl::bindArguments(
            [func=std::forward<TFunc>(func)](BuildParams const& params,
                bq::signal::Signal<T, avg::Vector2f> size, auto&&... ts)
            {
                auto sharedSize = size.share();
                auto builder = func(sharedSize,
                        provider::invokeParamProvider(ts, params)...);

                return builder(sharedSize);
            },
            std::forward<Ts>(ts)...
            ));
    }

    inline auto makeBuilder()
    {
        return makeBuilder(
                [](BuildParams params, auto size)
                {
                    return makeElement(size)
                        .setParams(std::move(params))
                        ;
                },
                BuildParams{},
                bq::signal::constant(avg::Vector2f(0.5f, 0.5f))
                );
    }

    template <typename T>
    auto makeBuilderFromElement(Element<T> element)
    {
        auto size = element.getSize().clone().share();
        auto buildParams = element.getParams();

        auto builder = makeBuilder(
                [element](BuildParams params, auto const& /*size*/)
                {
                    return btl::clone(element)
                        .setParams(params);
                },
                std::move(buildParams),
                bq::signal::constant(avg::Vector2f(0.5f, 0.5f))
                );

        builder.setPureLayout(pureLayoutFromSize(
                    bq::signal::AnySignal<avg::Vector2f>(std::move(size))));
        return builder;
    }

    template <typename TBuilder, typename = typename std::enable_if
        <
            IsBuilder<TBuilder>::value
        >::type>
    auto copy(TBuilder&& builder) -> btl::decay_t<TBuilder>
    {
        return { std::forward<TBuilder>(builder) };
    }
} // namespace bqui::widget

