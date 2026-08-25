#pragma once

#include "element.h"

#include "bqui/provider/paramprovider.h"

#include "bqui/buildparams.h"
#include "bqui/simplesizehint.h"
#include "bqui/sizehint.h"
#include "bqui/widget/boxvariables.h"
#include "bqui/widget/guide.h"

#include <bq/signal/signal.h>

#include <avg/transform.h>

#include <btl/cloneoncopy.h>

#include <functional>
#include <type_traits>

namespace bqui::widget
{
    template <typename TFunc, typename TSizeHint>
    class Builder;

    struct AnyBuilder;

    using BuilderBase = Builder<
        std::function<widget::AnyElement(BuildParams params,
                bq::signal::AnySignal<avg::Vector2f>)>,
        bq::signal::AnySignal<SizeHint>
        >;

    template <typename T>
    using IsBuilder = typename std::is_convertible<T, AnyBuilder>::type;

    template <typename TFunc, typename TSizeHint, typename = std::enable_if_t<
        std::is_invocable_r_v<
            widget::AnyElement, TFunc, BuildParams, bq::signal::AnySignal<avg::Vector2f>>
        >
    >
    auto makeBuilder(TFunc&& func, TSizeHint&& sizeHint,
            BuildParams params, bq::signal::AnySignal<avg::Vector2f> gravity)
    {
        return Builder<std::decay_t<TFunc>, std::decay_t<TSizeHint>>(
                std::forward<TFunc>(func),
                std::forward<TSizeHint>(sizeHint),
                std::move(params),
                std::move(gravity)
                );
    }

    template <typename TFunc, typename TSizeHint>
    class Builder
    {
    public:
        using SizeHintType = std::decay_t<TSizeHint>;

        Builder(TFunc func, TSizeHint sizeHint, BuildParams params,
                bq::signal::AnySignal<avg::Vector2f> gravity) :
            func_(std::move(func)),
            sizeHint_(std::move(sizeHint)),
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
            return std::invoke(*func_, std::move(buildParams_), std::move(size));
        }

        template <typename TSignalSizeHint>
        auto setSizeHint(TSignalSizeHint sizeHint) &&
        {
            auto builder = makeBuilder(
                    std::move(*func_),
                    std::move(sizeHint),
                    std::move(buildParams_),
                    std::move(gravity_)
                    );
            builder.setBoxVariables(box_);
            builder.setGuideAlignments(guideAlignments_);
            return builder;
        }

        SizeHintType getSizeHint() const
        {
            return sizeHint_->clone();
        }

        /**
         * @brief The four edge variables that name this widget's box to the
         * constraint solver. Stable for the builder's lifetime and preserved
         * across a copy, a size-hint change and type erasure.
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

        /**
         * @brief The guide alignments this widget requests, for its container
         * to resolve against a shared line. Preserved across a copy, a
         * size-hint change and type erasure, exactly as the box variables are.
         */
        std::vector<GuideAlignment> const& getGuideAlignments() const
        {
            return guideAlignments_;
        }

        /**
         * @brief Replaces this widget's guide alignments wholesale, used to
         * carry them across a rebuild that mints a fresh builder.
         */
        void setGuideAlignments(std::vector<GuideAlignment> alignments)
        {
            guideAlignments_ = std::move(alignments);
        }

        /**
         * @brief Records one more guide alignment on a copy of this builder.
         */
        auto addGuideAlignment(GuideAlignment alignment)
        {
            auto copy = clone();
            copy.guideAlignments_.push_back(std::move(alignment));
            return copy;
        }

        auto setBuildParams(BuildParams params) &&
        {
            auto builder = makeBuilder([params=std::move(buildParams_),
                    func=std::move(func_)](BuildParams oldParams, auto size)
                {
                    return (*func)(params, std::move(size)).setParams(oldParams);
                },
                std::move(*sizeHint_),
                std::move(params),
                std::move(gravity_)
                );
            builder.setBoxVariables(box_);
            builder.setGuideAlignments(guideAlignments_);
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
            return copy;
        }

        auto setGravity(avg::Vector2f gravity)
        {
            return setGravity(bq::signal::constant(std::move(gravity)));
        }

        bq::signal::AnySignal<avg::Vector2f> getGravity() const
        {
            return gravity_;
        }

        operator BuilderBase() &&
        {
            BuilderBase base(
                    std::move(*func_),
                    std::move(*sizeHint_),
                    std::move(buildParams_),
                    std::move(gravity_)
                    );
            base.setBoxVariables(box_);
            base.setGuideAlignments(guideAlignments_);
            return base;
        }

    protected:
        btl::CloneOnCopy<TFunc> func_;
        btl::CloneOnCopy<TSizeHint> sizeHint_;
        BuildParams buildParams_;
        bq::signal::AnySignal<avg::Vector2f> gravity_ =
            bq::signal::constant(avg::Vector2f(0.5f, 0.5f));
        BoxVariables box_;
        std::vector<GuideAlignment> guideAlignments_;
    };

    struct AnyBuilder : Builder<std::function<widget::AnyElement(
            BuildParams, bq::signal::AnySignal<avg::Vector2f>)>,
            bq::signal::AnySignal<SizeHint>>
    {
        template <typename TFunc, typename TSizeHint>
        static auto castBuilder(Builder<TFunc, TSizeHint> base)
        {
            auto sizeHint = base.getSizeHint();

            return std::move(base)
                .setSizeHint(std::move(sizeHint).template cast<SizeHint>())
                ;
        }

        AnyBuilder(AnyBuilder const&) = default;
        AnyBuilder(AnyBuilder&&) noexcept = default;

        AnyBuilder& operator=(AnyBuilder const&) = default;
        AnyBuilder& operator=(AnyBuilder&&) noexcept = default;

        template <typename TFunc, typename TSizeHint>
        AnyBuilder(Builder<TFunc, TSizeHint> base) :
            BuilderBase(castBuilder(std::move(base)))
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
                bq::signal::constant(defaultSizeHint()),
                BuildParams{},
                bq::signal::constant(avg::Vector2f(0.5f, 0.5f))
                );
    }

    template <typename T>
    auto makeBuilderFromElement(Element<T> element)
    {
        auto size = element.getSize().clone();
        auto buildParams = element.getParams();

        return makeBuilder(
                [element](BuildParams params, auto const& /*size*/)
                {
                    return btl::clone(element)
                        .setParams(params);
                },
                std::move(size).map([](auto size)
                    {
                        return simpleSizeHint(size[0], size[1]);
                    }),
                std::move(buildParams),
                bq::signal::constant(avg::Vector2f(0.5f, 0.5f))
                );
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

