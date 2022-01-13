#pragma once

#include "instancemodifier.h"
#include "instance.h"

#include "reactive/simplesizehint.h"
#include "reactive/sizehint.h"

#include "reactive/signal/cast.h"
#include "reactive/signal/share.h"
#include "reactive/signal/cache.h"
#include "reactive/signal/changed.h"
#include "reactive/signal/tee.h"
#include "reactive/signal/constant.h"
#include "reactive/signal/signal.h"

#include <avg/transform.h>

#include <btl/pushback.h>
#include <btl/tuplereduce.h>
#include <btl/all.h>
#include <btl/not.h>

#include <functional>
#include <deque>

namespace reactive::widget
{
    template <typename TTupleMaps, typename TSizeHint>
    class Builder;

    struct AnyBuilder;

    using BuilderBase = Builder<
        std::tuple<widget::AnyInstanceModifier>, AnySignal<SizeHint>
        >;

    template <typename T>
    using IsBuilder = typename std::is_convertible<T, AnyBuilder>::type;

    template <typename T>
    struct IsTupleMaps : std::false_type {};

    template <typename... Ts>
    struct IsTupleMaps<std::tuple<Ts...>> : btl::All<widget::IsInstanceModifier<Ts>...> {};

    namespace detail
    {
        struct EvaluateBuilderMapper
        {
            template <typename T, typename U>
            auto operator()(T&& initial, U&& map) const
            -> decltype(
                    std::forward<decltype(map)>(map)(
                        std::forward<decltype(initial)>(initial)
                        )
                    )
            {
                return std::forward<decltype(map)>(map)(
                        std::forward<decltype(initial)>(initial)
                        );
            }
        };

        template <typename T, typename TMaps>
        auto evaluateBuilder(
                Signal<T, avg::Vector2f> size,
                TMaps&& maps)
        -> decltype(
            btl::tuple_reduce(
                    widget::makeInstance(std::move(size)),
                    std::forward<TMaps>(maps),
                    EvaluateBuilderMapper()
                    )
            )
        {
            return btl::tuple_reduce(
                    widget::makeInstance(std::move(size)),
                    std::forward<TMaps>(maps),
                    EvaluateBuilderMapper()
                    );
        }
    }


    template <typename TTupleMaps, typename TSizeHint,
             typename = std::enable_if_t
                 <
                    btl::All<
                        //IsTupleMaps<TTupleMaps>,
                        IsSizeHint<signal::SignalType<TSizeHint>>
                    >::value
                 >>
    auto makeBuilder(TTupleMaps maps, TSizeHint sizeHint)
    {
        return Builder<std::decay_t<TTupleMaps>, std::decay_t<TSizeHint>>(
                std::forward<TTupleMaps>(maps),
                std::move(sizeHint)
                );
    }

    template <typename TTupleMaps, typename TSizeHint>
    class Builder
    {
    public:
        using SizeHintType = btl::decay_t<TSizeHint>;

        Builder(TTupleMaps maps, TSizeHint sizeHint) :
            maps_(std::move(maps)),
            sizeHint_(std::move(sizeHint))
        {
        }

        Builder<TTupleMaps, TSizeHint> clone() const
        {
            return *this;
        }

        Builder<TTupleMaps, TSizeHint>(
                Builder<TTupleMaps, TSizeHint> const&) = default;
        Builder<TTupleMaps, TSizeHint>& operator=(
                Builder<TTupleMaps, TSizeHint> const&) = default;

        Builder<TTupleMaps, TSizeHint>(
                Builder<TTupleMaps, TSizeHint>&&) noexcept = default;
        Builder<TTupleMaps, TSizeHint>& operator=(
                Builder<TTupleMaps, TSizeHint>&&) noexcept = default;

        template <typename T>
        auto operator()(Signal<T, avg::Vector2f> size) &&
        {
            return detail::evaluateBuilder(
                    std::move(size),
                    std::move(*maps_)
                    );
        }

        template <typename TFunc>
        auto map(TFunc&& func) &&
        -> decltype(
                makeBuilder(
                    btl::pushBack(
                        std::move(std::declval<btl::decay_t<TTupleMaps>>()),
                        std::forward<TFunc>(func)
                        ),
                    std::move(std::declval<btl::decay_t<TSizeHint>>())
                ))
        {
            return makeBuilder(
                    btl::pushBack(std::move(*maps_), std::forward<TFunc>(func)),
                    std::move(*sizeHint_)
                    );
        }

        template <typename TFunc>
        auto preMap(TFunc&& func) &&
        {
            return makeBuilder(
                    btl::pushFront(std::move(*maps_), std::forward<TFunc>(func)),
                    std::move(*sizeHint_)
                    );
        }

        template <typename TSignalSizeHint>
        auto setSizeHint(TSignalSizeHint sizeHint) &&
        {
            return makeBuilder(
                    std::move(*maps_),
                    std::move(sizeHint)
                    );
        }

        SizeHintType getSizeHint() const
        {
            return sizeHint_->clone();
        }

        operator BuilderBase() &&
        {
            widget::AnyInstanceModifier f = widget::makeInstanceSignalModifier(
                [](auto w, auto maps) mutable
                {
                    return btl::tuple_reduce(std::move(w), std::move(*maps),
                        [](auto&& initial, auto&& map) mutable
                        {
                            return std::forward<decltype(map)>(map)(
                                std::forward<decltype(initial)>(initial)
                                );
                        });
                },
                std::move(maps_)
                );

            return BuilderBase(
                    std::make_tuple(std::move(f)),
                    std::move(*sizeHint_)
                    );
        }

    protected:
        btl::CloneOnCopy<btl::decay_t<TTupleMaps>> maps_;
        btl::CloneOnCopy<btl::decay_t<TSizeHint>> sizeHint_;
    };

    struct AnyBuilder : Builder<std::tuple<AnyInstanceModifier>, AnySignal<SizeHint>>
    {
        template <typename TTupleMaps, typename TSizeHint>
        static auto castBuilder(Builder<TTupleMaps, TSizeHint> base)
        {
            auto sizeHint = base.getSizeHint();

            return std::move(base)
                .setSizeHint(signal::cast<SizeHint>(std::move(sizeHint)));
        }

        AnyBuilder(AnyBuilder const&) = default;
        AnyBuilder(AnyBuilder&&) noexcept = default;

        AnyBuilder& operator=(AnyBuilder const&) = default;
        AnyBuilder& operator=(AnyBuilder&&) noexcept = default;

        template <typename TTupleMaps, typename TSizeHint>
        AnyBuilder(Builder<TTupleMaps, TSizeHint> base) :
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
                std::tuple<>(),
                signal::constant(simpleSizeHint(100.0f, 100.0f))
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

