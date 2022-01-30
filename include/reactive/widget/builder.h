#pragma once

#include "instancemodifier.h"
#include "instance.h"
#include "buildparams.h"

#include "reactive/simplesizehint.h"
#include "reactive/sizehint.h"

#include "reactive/signal/cast.h"
#include "reactive/signal/share.h"
#include "reactive/signal/tee.h"
#include "reactive/signal/constant.h"
#include "reactive/signal/signal.h"

#include <avg/transform.h>

#include <btl/cloneoncopy.h>

#include <functional>
#include <type_traits>

namespace reactive::widget
{
    template <typename TFunc, typename TSizeHint>
    class Builder;

    struct AnyBuilder;

    using BuilderBase = Builder<
        widget::AnyInstanceModifier, AnySignal<SizeHint>
        >;

    template <typename T>
    using IsBuilder = typename std::is_convertible<T, AnyBuilder>::type;

    template <typename TModifier, typename TSizeHint>
    auto makeBuilder(InstanceModifier<TModifier> modifier,
            TSizeHint&& sizeHint, BuildParams params)
    {
        return Builder<InstanceModifier<TModifier>, std::decay_t<TSizeHint>>(
                std::move(modifier),
                std::forward<TSizeHint>(sizeHint),
                std::move(params)
                );
    }

    template <typename TModifier, typename TSizeHint>
    class Builder
    {
    public:
        using SizeHintType = std::decay_t<TSizeHint>;

        Builder(TModifier modifier, TSizeHint sizeHint, BuildParams params) :
            modifier_(std::move(modifier)),
            sizeHint_(std::move(sizeHint)),
            buildParams_(std::move(params))
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
        auto operator()(Signal<T, avg::Vector2f> size) &&
        {
            return makeInstance(std::move(size))
                | std::move(*modifier_)
                ;
        }

        template <typename TFunc, typename = std::enable_if_t<
            std::is_invocable_r_v<AnySignal<Instance>, TFunc, AnySignal<Instance>>
            >>
        auto map(TFunc&& func) &&
        {
            return makeBuilder(
                    detail::makeInstanceSignalModifierUnchecked(
                        [](auto instance, auto&& modifier, auto&& func)
                        {
                            return std::move(instance)
                                | std::forward<decltype(modifier)>(modifier)
                                | std::forward<decltype(func)>(func)
                                ;
                        },
                        std::move(*modifier_),
                        std::forward<TFunc>(func)
                        ),
                    std::move(*sizeHint_),
                    std::move(buildParams_)
                    );
        }

        template <typename TFunc, typename = std::enable_if_t<
            std::is_invocable_r_v<AnySignal<Instance>, TFunc, AnySignal<Instance>>
            >>
        auto preMap(TFunc&& func) &&
        {
            return makeBuilder(
                    detail::makeInstanceSignalModifierUnchecked(
                        [](auto instance, auto&& modifier, auto&& func)
                        {
                            return std::move(instance)
                                | std::forward<decltype(func)>(func)
                                | std::forward<decltype(modifier)>(modifier)
                                ;
                        },
                        std::move(*modifier_),
                        std::forward<TFunc>(func)
                        ),
                    std::move(*sizeHint_),
                    std::move(buildParams_)
                    );
        }

        template <typename TSignalSizeHint>
        auto setSizeHint(TSignalSizeHint sizeHint) &&
        {
            return makeBuilder(
                    std::move(*modifier_),
                    std::move(sizeHint),
                    std::move(buildParams_)
                    );
        }

        SizeHintType getSizeHint() const
        {
            return sizeHint_->clone();
        }

        auto setBuildParams(BuildParams params) &&
        {
            return makeBuilder(
                    std::move(*modifier_),
                    std::move(*sizeHint_),
                    std::move(params)
                    );
        }

        BuildParams const& getBuildParams() const
        {
            return buildParams_;
        }

        operator BuilderBase() &&
        {
            return BuilderBase(
                    std::move(*modifier_),
                    std::move(*sizeHint_),
                    std::move(buildParams_)
                    );
        }

    protected:
        btl::CloneOnCopy<TModifier> modifier_;
        btl::CloneOnCopy<TSizeHint> sizeHint_;
        BuildParams buildParams_;
    };

    struct AnyBuilder : Builder<AnyInstanceModifier, AnySignal<SizeHint>>
    {
        template <typename TModifier, typename TSizeHint>
        static auto castBuilder(Builder<TModifier, TSizeHint> base)
        {
            auto sizeHint = base.getSizeHint();

            return std::move(base)
                .setSizeHint(signal::cast<SizeHint>(std::move(sizeHint)))
                ;
        }

        AnyBuilder(AnyBuilder const&) = default;
        AnyBuilder(AnyBuilder&&) noexcept = default;

        AnyBuilder& operator=(AnyBuilder const&) = default;
        AnyBuilder& operator=(AnyBuilder&&) noexcept = default;

        template <typename TModifier, typename TSizeHint>
        AnyBuilder(Builder<TModifier, TSizeHint> base) :
            BuilderBase(castBuilder(std::move(base)))
        {
        }

        AnyBuilder clone() const
        {
            return *this;
        }
    };

    inline auto makeBuilder()
    {
        return makeBuilder(
                detail::makeInstanceSignalModifierUnchecked(
                    [](auto instance) { return instance; }),
                signal::constant(simpleSizeHint(100.0f, 100.0f)),
                BuildParams{}
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
}

