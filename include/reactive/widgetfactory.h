#pragma once

#include "widget/widgettransformer.h"

#include "simplesizehint.h"
#include "sizehint.h"
#include "widget.h"

#include "signal/cast.h"
#include "signal/share.h"
#include "signal/cache.h"
#include "signal/changed.h"
#include "signal/tee.h"
#include "signal/constant.h"
#include "signal.h"

#include <avg/transform.h>

#include <btl/pushback.h>
#include <btl/tuplereduce.h>
#include <btl/all.h>
#include <btl/not.h>

#include <functional>
#include <deque>

namespace reactive
{
    template <typename TTupleMaps, typename TSizeHint>
    class WidFac;

    using FactoryMapWidget = std::function<Widget(Widget)>;
    using WidgetFactoryBase = WidFac<
        std::tuple<widget::WidgetTransformer<void>>, Signal<SizeHint>
        >;
    struct WidgetFactory;
    using FactoryMap = std::function<WidgetFactory(WidgetFactory)>;

    template <typename T>
    using IsWidgetFactory = typename std::is_convertible<T, WidgetFactory>::type;

    template <typename T>
    using IsFactoryMap = typename std::is_convertible<T, FactoryMap>::type;

    template <typename T>
    struct IsTupleMaps : std::false_type {};

    template <typename... Ts>
    struct IsTupleMaps<std::tuple<Ts...>> : btl::All<widget::IsWidgetTransformer<Ts>...> {};

    template <typename TFunc>
    struct FactoryMapWrapper
    {
        template <typename TFactory, typename = std::enable_if_t<
            IsWidgetFactory<TFactory>::value
            >
        >
        auto operator()(TFactory factory)
        {
            return std::move(*func)(std::move(factory));
        }

        btl::CloneOnCopy<std::decay_t<TFunc>> func;
    };

    template <typename TFunc, typename = std::enable_if_t<
        IsFactoryMap<TFunc>::value
        >
    >
    auto mapFactory(TFunc&& func)
    {
        return FactoryMapWrapper<std::decay_t<TFunc>>{std::forward<TFunc>(func)};
    }

    namespace detail
    {
        struct EvaluateWidgetFactoryMapper
        {
            template <typename T, typename U>
            auto operator()(T&& initial, U&& map) const
            -> decltype(
                    std::forward<decltype(map)>(map)(
                        std::forward<decltype(initial)>(initial)
                        ).first
                    )
            {
                return std::forward<decltype(map)>(map)(
                        std::forward<decltype(initial)>(initial)
                        ).first;
            }
        };

        template <typename T, typename U, typename TMaps>
        auto evaluateWidgetFactory(
                Signal<DrawContext, T> drawContext,
                Signal<avg::Vector2f, U> size,
                TMaps&& maps)
        -> decltype(
            btl::tuple_reduce(
                    makeWidget( std::move(drawContext), std::move(size)),
                    std::forward<TMaps>(maps),
                    EvaluateWidgetFactoryMapper()
                    )
            )
        {
            return btl::tuple_reduce(
                    makeWidget(std::move(drawContext), std::move(size)),
                    std::forward<TMaps>(maps),
                    EvaluateWidgetFactoryMapper()
                    );
        }
    }


    template <typename TTupleMaps, typename TSizeHint,
             typename = std::enable_if_t
                 <
                    btl::All<
                        //IsTupleMaps<TTupleMaps>,
                        IsSizeHint<SignalType<TSizeHint>>
                    >::value
                 >>
    auto makeWidFac(TTupleMaps maps, TSizeHint sizeHint)
    {
        return WidFac<std::decay_t<TTupleMaps>, std::decay_t<TSizeHint>>(
                std::forward<TTupleMaps>(maps),
                std::move(sizeHint)
                );
    }

    template <typename TTupleMaps, typename TSizeHint>
    class WidFac
    {
    public:
        using SizeHintType = btl::decay_t<TSizeHint>;

        WidFac(TTupleMaps maps, TSizeHint sizeHint) :
            maps_(std::move(maps)),
            sizeHint_(std::move(sizeHint))
        {
        }

        WidFac clone() const
        {
            return *this;
        }

        WidFac(WidFac const&) = default;
        WidFac& operator=(WidFac const&) = default;

        WidFac(WidFac&&) noexcept = default;
        WidFac& operator=(WidFac&&) noexcept = default;

        template <typename T, typename U>
        auto operator()(
                Signal<DrawContext, T> drawContext,
                Signal<avg::Vector2f, U> size
                ) &&
        {
            return detail::evaluateWidgetFactory(
                    std::move(drawContext),
                    std::move(size),
                    std::move(*maps_)
                    );
        }

        template <typename TFunc>
        auto map(TFunc&& func) &&
        -> decltype(
                makeWidFac(
                    btl::pushBack(
                        std::move(std::declval<btl::decay_t<TTupleMaps>>()),
                        std::forward<TFunc>(func)
                        ),
                    std::move(std::declval<btl::decay_t<TSizeHint>>())
                ))
        {
            return makeWidFac(
                    btl::pushBack(std::move(*maps_), std::forward<TFunc>(func)),
                    std::move(*sizeHint_)
                    );
        }

        template <typename TFunc>
        auto preMap(TFunc&& func) &&
        {
            return makeWidFac(
                    btl::pushFront(std::move(*maps_), std::forward<TFunc>(func)),
                    std::move(*sizeHint_)
                    );
        }

        template <typename TSignalSizeHint>
        auto setSizeHint(TSignalSizeHint sizeHint) &&
        {
            return makeWidFac(
                    std::move(*maps_),
                    std::move(sizeHint)
                    );
        }

        SizeHintType getSizeHint() const
        {
            return sizeHint_->clone();
        }

        operator WidgetFactoryBase() &&
        {
            widget::WidgetTransformer<void> f = widget::makeWidgetTransformer(
                [maps = std::move(maps_)]
                (auto w) mutable
                {
                    return widget::makeWidgetTransformerResult(
                            btl::tuple_reduce(std::move(w), std::move(*maps),
                            [](auto&& initial, auto&& map) mutable
                            {
                                return std::forward<decltype(map)>(map)(
                                    std::forward<decltype(initial)>(initial)
                                    ).first;
                            }));
                });

            return WidgetFactoryBase(
                    std::make_tuple(std::move(f)),
                    std::move(*sizeHint_)
                    );
        }

    private:
        btl::CloneOnCopy<btl::decay_t<TTupleMaps>> maps_;
        btl::CloneOnCopy<btl::decay_t<TSizeHint>> sizeHint_;
    };

    struct WidgetFactory : WidgetFactoryBase
    {
        WidgetFactory(WidgetFactoryBase base) :
            WidgetFactoryBase(std::move(base))
        {
        }

        template <typename TTupleMaps, typename TSizeHint>
        static auto castFactory(WidFac<TTupleMaps, TSizeHint> base)
        {
            auto sizeHint = base.getSizeHint();

            return std::move(base)
                .setSizeHint(signal::cast<SizeHint>(std::move(sizeHint)));
        }

        template <typename TTupleMaps, typename TSizeHint>
        WidgetFactory(WidFac<TTupleMaps, TSizeHint> base) :
            WidgetFactoryBase(castFactory(std::move(base)))
        {
        }

        WidgetFactory clone() const
        {
            return WidgetFactoryBase::clone();
        }
    };

    inline auto makeWidgetFactory()
    {
        return makeWidFac(
                std::tuple<>(),
                signal::constant(simpleSizeHint(100.0f, 100.0f))
                );
    }

    template <typename TFactory, typename TFunc, typename = typename
        std::enable_if
        <
            btl::All<
                IsWidgetFactory<TFactory>,
                widget::IsWidgetTransformer<TFunc>
            >::value
        >::type>
    auto map(TFactory factory, TFunc func)
    {
        return std::move(factory)
            .map(std::move(func));
    }

    template <typename TFactory, typename TFunc, typename = typename
        std::enable_if
        <
            btl::All<
                IsWidgetFactory<TFactory>,
                widget::IsWidgetTransformer<TFunc>
            >::value
        >::type>
    auto preMap(TFactory factory, TFunc func)
    //-> WidgetFactory
    {
        return std::move(factory)
            .preMap(std::move(func));
    }

    template <typename TFunc, typename = typename
        std::enable_if
        <
            widget::IsWidgetTransformer<TFunc>::value
        >::type>
    auto mapFactoryWidget(TFunc f)
    //-> FactoryMap
    {
        auto g = [f = std::move(f)](auto factory)
            //-> WidgetFactory
        {
            return map(std::move(factory), f);
        };

        return mapFactory(std::move(g));
    }

    template <typename TFunc, typename = typename
        std::enable_if
        <
            widget::IsWidgetTransformer<TFunc>::value
        >::type>
    auto preMapFactory(TFunc f)
    //-> FactoryMap
    {
        auto g = [f = std::move(f)](auto factory)
            // -> WidgetFactory
        {
            return preMap(std::move(factory), f);
        };

        return mapFactory(std::move(g));
    }

    template <typename TFactoryMapL, typename TFactoryMapR,
             typename = typename std::enable_if<
                 btl::All<
                    IsFactoryMap<TFactoryMapL>,
                    IsFactoryMap<TFactoryMapR>
                 >::value
            >::type
        >
    inline auto operator>>(TFactoryMapL&& lhs, TFactoryMapR&& rhs)
        // -> FactoryMap
    {
        return mapFactory(
                [lhs=std::forward<TFactoryMapL>(lhs),
                rhs=std::forward<TFactoryMapR>(rhs)]
                (auto factory) mutable
                {
                    return rhs(lhs(std::move(factory)));
                });
    }

    template <typename TFactoryMap, typename... Ts>
    auto operator|(WidFac<Ts...> factory, FactoryMapWrapper<TFactoryMap> f)
    -> decltype(
            std::move(f)(std::move(factory))
            )
        // -> WidgetFactory
    {
        return std::move(f)(std::move(factory));
    }

    template <typename TWidgetTransformer, typename... Ts>
    auto operator|(WidFac<Ts...> factory, TWidgetTransformer&& f)
    -> decltype(std::move(factory)
            .map(std::forward<TWidgetTransformer>(f))
            )
        //-> WidgetFactory
    {
        return std::move(factory)
            .map(std::forward<TWidgetTransformer>(f));
    }

    template <typename TSignalSizeHint, typename = std::enable_if_t<
        IsSizeHint<SignalType<TSignalSizeHint>>::value
        >
    >
    auto setSizeHint(TSignalSizeHint sizeHint)
        //-> FactoryMap;
    {
        auto f = [sh = btl::cloneOnCopy(std::move(sizeHint))]
            (auto factory) // -> WidgetFactory
        {
            return std::move(factory)
                .setSizeHint(sh->clone())
                ;
        };

        return mapFactory(std::move(f));
    }

    template <typename TWidgetFactory, typename = typename std::enable_if
        <
            IsWidgetFactory<TWidgetFactory>::value
        >::type>
    auto copy(TWidgetFactory&& factory) -> btl::decay_t<TWidgetFactory>
    {
        return { std::forward<TWidgetFactory>(factory) };
    }
}

