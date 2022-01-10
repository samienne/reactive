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
    class Bldr;

    using BuilderBase = Bldr<
        std::tuple<widget::AnyInstanceModifier>, AnySignal<SizeHint>
        >;
    struct Builder;
    using BuiderModifier = std::function<Builder(Builder)>;

    template <typename T>
    using IsBuilder = typename std::is_convertible<T, Builder>::type;

    template <typename T>
    using IsBuilderModifier = typename std::is_convertible<T, BuiderModifier>::type;

    template <typename T>
    struct IsTupleMaps : std::false_type {};

    template <typename... Ts>
    struct IsTupleMaps<std::tuple<Ts...>> : btl::All<widget::IsInstanceModifier<Ts>...> {};

    template <typename TFunc>
    struct BuilderModifierWrapper
    {
        template <typename T, typename = std::enable_if_t<
            IsBuilder<T>::value
            >
        >
        auto operator()(T builder)
        {
            return std::move(*func)(std::move(builder));
        }

        btl::CloneOnCopy<std::decay_t<TFunc>> func;
    };

    template <typename TFunc, typename = std::enable_if_t<
        IsBuilderModifier<TFunc>::value
        >
    >
    auto makeBuilderModifier(TFunc&& func)
    {
        return BuilderModifierWrapper<std::decay_t<TFunc>>{std::forward<TFunc>(func)};
    }

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
    auto makeBldr(TTupleMaps maps, TSizeHint sizeHint)
    {
        return Bldr<std::decay_t<TTupleMaps>, std::decay_t<TSizeHint>>(
                std::forward<TTupleMaps>(maps),
                std::move(sizeHint)
                );
    }

    template <typename TTupleMaps, typename TSizeHint>
    class Bldr
    {
    public:
        using SizeHintType = btl::decay_t<TSizeHint>;

        Bldr<TTupleMaps, TSizeHint>(TTupleMaps maps, TSizeHint sizeHint) :
            maps_(std::move(maps)),
            sizeHint_(std::move(sizeHint))
        {
        }

        Bldr<TTupleMaps, TSizeHint> clone() const
        {
            return *this;
        }

        Bldr<TTupleMaps, TSizeHint>(Bldr<TTupleMaps, TSizeHint> const&) = default;
        Bldr<TTupleMaps, TSizeHint>& operator=(Bldr<TTupleMaps, TSizeHint> const&) = default;

        Bldr<TTupleMaps, TSizeHint>(Bldr<TTupleMaps, TSizeHint>&&) noexcept = default;
        Bldr<TTupleMaps, TSizeHint>& operator=(Bldr<TTupleMaps, TSizeHint>&&) noexcept = default;

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
                makeBldr(
                    btl::pushBack(
                        std::move(std::declval<btl::decay_t<TTupleMaps>>()),
                        std::forward<TFunc>(func)
                        ),
                    std::move(std::declval<btl::decay_t<TSizeHint>>())
                ))
        {
            return makeBldr(
                    btl::pushBack(std::move(*maps_), std::forward<TFunc>(func)),
                    std::move(*sizeHint_)
                    );
        }

        template <typename TFunc>
        auto preMap(TFunc&& func) &&
        {
            return makeBldr(
                    btl::pushFront(std::move(*maps_), std::forward<TFunc>(func)),
                    std::move(*sizeHint_)
                    );
        }

        template <typename TSignalSizeHint>
        auto setSizeHint(TSignalSizeHint sizeHint) &&
        {
            return makeBldr(
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

    private:
        btl::CloneOnCopy<btl::decay_t<TTupleMaps>> maps_;
        btl::CloneOnCopy<btl::decay_t<TSizeHint>> sizeHint_;
    };

    struct Builder : BuilderBase
    {
        Builder(BuilderBase base) :
            BuilderBase(std::move(base))
        {
        }

        template <typename TTupleMaps, typename TSizeHint>
        static auto castBuilder(Bldr<TTupleMaps, TSizeHint> base)
        {
            auto sizeHint = base.getSizeHint();

            return std::move(base)
                .setSizeHint(signal::cast<SizeHint>(std::move(sizeHint)));
        }

        template <typename TTupleMaps, typename TSizeHint>
        Builder(Bldr<TTupleMaps, TSizeHint> base) :
            BuilderBase(castBuilder(std::move(base)))
        {
        }

        Builder clone() const
        {
            return BuilderBase::clone();
        }
    };

    inline auto makeBuilder()
    {
        return makeBldr(
                std::tuple<>(),
                signal::constant(simpleSizeHint(100.0f, 100.0f))
                );
    }

    template <typename TBuilder, typename TFunc, typename = typename
        std::enable_if
        <
            btl::All<
                IsBuilder<TBuilder>,
                widget::IsInstanceModifier<TFunc>
            >::value
        >::type>
    auto map(TBuilder builder, TFunc func)
    {
        return std::move(builder)
            .map(std::move(func));
    }

    template <typename TBuilder, typename TFunc, typename = typename
        std::enable_if
        <
            btl::All<
                IsBuilder<TBuilder>,
                widget::IsInstanceModifier<TFunc>
            >::value
        >::type>
    auto preMap(TBuilder builder, TFunc func)
    //-> Builder
    {
        return std::move(builder)
            .preMap(std::move(func));
    }

    template <typename TFunc>
    auto makeBuilderModifier(widget::InstanceModifier<TFunc> f)
    //-> BuiderModifier
    {
        auto g = [f = std::move(f)](auto builder)
            //-> Builder
        {
            return map(std::move(builder), f);
        };

        return makeBuilderModifier(std::move(g));
    }

    template <typename TFunc>
    auto preMapBuilder(widget::InstanceModifier<TFunc> f)
    //-> BuiderModifier
    {
        auto g = [f = std::move(f)](auto builder)
            // -> Builder
        {
            return preMap(std::move(builder), f);
        };

        return makeBuilderModifier(std::move(g));
    }

    template <typename TBuilderMapL, typename TBuilderMapR,
             typename = typename std::enable_if<
                 btl::All<
                    IsBuilderModifier<TBuilderMapL>,
                    IsBuilderModifier<TBuilderMapR>
                 >::value
            >::type
        >
    inline auto operator>>(TBuilderMapL&& lhs, TBuilderMapR&& rhs)
        // -> BuiderModifier
    {
        return makeBuilderModifier(
                [lhs=std::forward<TBuilderMapL>(lhs),
                rhs=std::forward<TBuilderMapR>(rhs)]
                (auto builder) mutable
                {
                    return rhs(lhs(std::move(builder)));
                });
    }

    template <typename TBuilderMap, typename... Ts>
    auto operator|(Bldr<Ts...> builder, BuilderModifierWrapper<TBuilderMap> f)
    -> decltype(
            std::move(f)(std::move(builder))
            )
        // -> Builder
    {
        return std::move(f)(std::move(builder));
    }

    template <typename TInstanceModifier, typename... Ts>
    auto operator|(Bldr<Ts...> builder, TInstanceModifier&& f)
    -> decltype(std::move(builder)
            .map(std::forward<TInstanceModifier>(f))
            )
        //-> Builder
    {
        return std::move(builder)
            .map(std::forward<TInstanceModifier>(f));
    }

    template <typename TSignalSizeHint, typename = std::enable_if_t<
        IsSizeHint<signal::SignalType<TSignalSizeHint>>::value
        >
    >
    auto setSizeHint(TSignalSizeHint sizeHint)
        //-> BuiderModifier;
    {
        auto f = [sh = btl::cloneOnCopy(std::move(sizeHint))]
            (auto builder) // -> Builder
        {
            return std::move(builder)
                .setSizeHint(sh->clone())
                ;
        };

        return makeBuilderModifier(std::move(f));
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

