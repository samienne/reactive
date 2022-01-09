#pragma once

#include "widget/widgetmodifier.h"

#include "simplesizehint.h"
#include "sizehint.h"
#include "widget.h"

#include "signal/cast.h"
#include "signal/share.h"
#include "signal/cache.h"
#include "signal/changed.h"
#include "signal/tee.h"
#include "signal/constant.h"
#include "signal/signal.h"

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
        std::tuple<widget::AnyWidgetModifier>, AnySignal<SizeHint>
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
    struct IsTupleMaps<std::tuple<Ts...>> : btl::All<widget::IsWidgetModifier<Ts>...> {};

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
                        )
                    )
            {
                return std::forward<decltype(map)>(map)(
                        std::forward<decltype(initial)>(initial)
                        );
            }
        };

        template <typename T, typename TMaps>
        auto evaluateWidgetFactory(
                Signal<T, avg::Vector2f> size,
                TMaps&& maps)
        -> decltype(
            btl::tuple_reduce(
                    makeWidget(std::move(size)),
                    std::forward<TMaps>(maps),
                    EvaluateWidgetFactoryMapper()
                    )
            )
        {
            return btl::tuple_reduce(
                    makeWidget(std::move(size)),
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
                        IsSizeHint<signal::SignalType<TSizeHint>>
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

        WidFac<TTupleMaps, TSizeHint>(TTupleMaps maps, TSizeHint sizeHint) :
            maps_(std::move(maps)),
            sizeHint_(std::move(sizeHint))
        {
        }

        WidFac<TTupleMaps, TSizeHint> clone() const
        {
            return *this;
        }

        WidFac<TTupleMaps, TSizeHint>(WidFac<TTupleMaps, TSizeHint> const&) = default;
        WidFac<TTupleMaps, TSizeHint>& operator=(WidFac<TTupleMaps, TSizeHint> const&) = default;

        WidFac<TTupleMaps, TSizeHint>(WidFac<TTupleMaps, TSizeHint>&&) noexcept = default;
        WidFac<TTupleMaps, TSizeHint>& operator=(WidFac<TTupleMaps, TSizeHint>&&) noexcept = default;

        template <typename T>
        auto operator()(Signal<T, avg::Vector2f> size) &&
        {
            return detail::evaluateWidgetFactory(
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
            widget::AnyWidgetModifier f = widget::makeWidgetSignalModifier(
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
                widget::IsWidgetModifier<TFunc>
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
                widget::IsWidgetModifier<TFunc>
            >::value
        >::type>
    auto preMap(TFactory factory, TFunc func)
    //-> WidgetFactory
    {
        return std::move(factory)
            .preMap(std::move(func));
    }

    template <typename TFunc>
    auto mapFactoryWidget(widget::WidgetModifier<TFunc> f)
    //-> FactoryMap
    {
        auto g = [f = std::move(f)](auto factory)
            //-> WidgetFactory
        {
            return map(std::move(factory), f);
        };

        return mapFactory(std::move(g));
    }

    template <typename TFunc>
    auto preMapFactory(widget::WidgetModifier<TFunc> f)
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

    template <typename TWidgetModifier, typename... Ts>
    auto operator|(WidFac<Ts...> factory, TWidgetModifier&& f)
    -> decltype(std::move(factory)
            .map(std::forward<TWidgetModifier>(f))
            )
        //-> WidgetFactory
    {
        return std::move(factory)
            .map(std::forward<TWidgetModifier>(f));
    }

    template <typename TSignalSizeHint, typename = std::enable_if_t<
        IsSizeHint<signal::SignalType<TSignalSizeHint>>::value
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

