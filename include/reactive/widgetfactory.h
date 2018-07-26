#pragma once

#include "sizehint.h"
#include "widgetmaps.h"
#include "widgetmap.h"
#include "widget.h"

#include "signal/cast.h"
#include "signal/share.h"
#include "signal/cache.h"
#include "signal/changed.h"
#include "signal/tee.h"
#include "signal.h"

#include <avg/transform.h>

#include <btl/pushback.h>
#include <btl/tuplereduce.h>
#include <btl/all.h>
#include <btl/not.h>

#include <fit/compose.h>

#include <functional>
#include <deque>

namespace reactive
{
    template <typename TTupleMaps, typename TSizeHint>
    class WidFac;

    using FactoryMapWidget = std::function<Widget(Widget)>;
    using WidgetFactoryBase = WidFac<std::tuple<WidgetMap>, Signal<SizeHint>>;
    struct WidgetFactory;
    using FactoryMap = std::function<WidgetFactory(WidgetFactory)>;

    template <typename T>
    using IsWidgetFactory = typename std::is_convertible<T, WidgetFactory>::type;

    template <typename T>
    using IsFactoryMap = typename std::is_convertible<T, FactoryMap>::type;

    template <typename T>
    struct IsTupleMaps : std::false_type {};

    template <typename... Ts>
    struct IsTupleMaps<std::tuple<Ts...>> : btl::All<IsWidgetMap<Ts>...> {};

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


    template <typename TTupleMaps, typename TSizeHint,
             typename = std::enable_if_t
                 <
                    btl::All<
                        IsTupleMaps<TTupleMaps>,
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

    private:
    public:
        WidFac(WidFac const&) = default;
        WidFac& operator=(WidFac const&) = default;

    public:
        WidFac(WidFac&&) noexcept = default;
        WidFac& operator=(WidFac&&) noexcept = default;

        template <typename TSignalInput>
        auto operator()(TSignalInput input) &&
        {
            return btl::tuple_reduce(
                    makeWidget(std::move(input)),
                    std::move(*maps_),
                    [](auto&& initial, auto&& map)
                    {
                        /*
                        static_assert(IsWidget<decltype(initial)>::value, "");
                        static_assert(IsWidgetMap<std::decay_t<decltype(map)>>::value,
                            "");
                        */

                        return std::forward<decltype(map)>(map)(
                            std::forward<decltype(initial)>(initial));
                    })
                    ;
        }

        template <typename TFunc, typename = std::enable_if_t<
            IsWidgetMap<TFunc>::value
            >>
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
            auto f = [maps = std::move(maps_)]
                (Widget w) mutable -> Widget
                {
                    return btl::tuple_reduce(std::move(w), std::move(*maps),
                            [](auto&& initial, auto&& map) mutable
                            {
                                return std::forward<decltype(map)>(map)(
                                    std::forward<decltype(initial)>(initial));
                            });
                };

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
                IsWidgetMap<TFunc>
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
                IsWidgetMap<TFunc>
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
            IsWidgetMap<TFunc>::value
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
            IsWidgetMap<TFunc>::value
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

    template <typename TFactoryMapL, typename TFactoryMapR>
    struct CombinedFactoryMap
    {
        CombinedFactoryMap(TFactoryMapL&& lhs, TFactoryMapR&& rhs) :
            lhs_(std::forward<TFactoryMapL>(lhs)),
            rhs_(std::forward<TFactoryMapR>(rhs))
        {
        }

        template <typename TFactory, typename = typename
            std::enable_if
            <
                IsWidgetFactory<TFactory>::value
            >::type>
        auto operator()(TFactory factory)
            //-> decltype(rhs_(lhs_(std::forward<Ts>(ts)...)))
        {
            return rhs_(lhs_(std::move(factory)));
        }

        btl::decay_t<TFactoryMapL> lhs_;
        btl::decay_t<TFactoryMapR> rhs_;
    };

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
        auto f = CombinedFactoryMap<TFactoryMapL, TFactoryMapR>(
                std::forward<TFactoryMapL>(lhs),
                std::forward<TFactoryMapR>(rhs));

        return f;
    }

    template <typename TFactory, typename TFactoryMap, typename = typename
        std::enable_if_t
        <
            btl::All<
                IsWidgetFactory<TFactory>,
                IsFactoryMap<TFactoryMap>,
                btl::Not<IsWidgetMap<TFactoryMap>>
            >::value
        >>
    auto operator|(TFactory factory, TFactoryMap f)
    -> decltype(
            std::move(f)(std::move(factory))
            )
        // -> WidgetFactory
    {
        return std::move(f)(std::move(factory));
    }

    template <typename TFactory, typename TWidgetMap, typename = typename
        std::enable_if
        <
            btl::All<
                IsWidgetFactory<TFactory>,
                IsWidgetMap<TWidgetMap>
            >::value
        >::type>
    auto operator|(TFactory factory, TWidgetMap f)
    -> decltype(std::move(factory)
            .map(std::move(f))
            )
        //-> WidgetFactory
    {
        return std::forward<TFactory>(factory)
            .map(std::forward<TWidgetMap>(f));
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

    inline auto trackSize(signal::InputHandle<ase::Vector2f> handle)
        //-> FactoryMap
    {
        auto f = [handle = std::move(handle)](auto widget)
            // -> Widget
        {
            auto obb = signal::tee(
                    widget.getObb(),
                    std::mem_fn(&avg::Obb::getSize),
                    handle);

            return std::move(widget)
                .setObb(std::move(obb))
                |
                makeWidgetMap<ObbTag, KeyboardInputTag>(
                        [](avg::Obb, std::vector<KeyboardInput> inputs)
                        {
                            // TODO: Remove this hack. This is needed to keep
                            // the obb signal in around.
                            return inputs;
                        });
        };

        static_assert(std::is_convertible<decltype(f), WidgetMap>::value, "");

        return mapWidget(f);
    }

    inline auto trackObb(signal::InputHandle<avg::Obb> handle)
        //-> FactoryMap
    {
        auto f = [handle = std::move(handle)](auto widget)
            // -> Widget
        {
            auto obb = signal::tee(widget.getObb(), handle);

            return std::move(widget)
                .setObb(std::move(obb))
                |
                makeWidgetMap<ObbTag, KeyboardInputTag>(
                        [](avg::Obb, std::vector<KeyboardInput> inputs)
                        {
                            // TODO: Remove this hack. This is needed to keep
                            // the obb signal in around.
                            return inputs;
                        });
        };

        static_assert(std::is_convertible<decltype(f), WidgetMap>::value, "");

        return mapWidget(f);
    }

    inline auto trackFocus(signal::InputHandle<bool> const& handle)
        // -> FactoryMap
    {
        auto f = [handle](auto widget)
        {
            auto anyHasFocus = [](std::vector<KeyboardInput> const& inputs)
                -> bool
            {
                for (auto&& input : inputs)
                    if (input.hasFocus())
                        return true;

                return false;
            };

            auto input = signal::tee(
                    signal::share(widget.getKeyboardInputs()),
                    anyHasFocus, handle);

            return std::move(widget)
                .setKeyboardInputs(std::move(input));
        };

        return mapWidget(std::move(f));
    }

    inline auto trackTheme(signal::InputHandle<widget::Theme> const& handle)
        //-> FactoryMap
    {
        auto f = [handle](auto widget)
            // -> Widget
        {
            auto theme = widget.getTheme();
            return std::move(widget)
                .setTheme(signal::tee(std::move(theme), handle));
        };

        return mapWidget(f);
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

