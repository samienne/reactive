#pragma once

#include "widgetfactory.h"

#include "signal/simpledelegate.h"
#include "signal/databind.h"
#include "signal/map.h"
#include "signal/mbind.h"
#include "signal/combine.h"
#include "signal/track.h"
#include "signal/join.h"

#include "signal.h"

#include <btl/sequence.h>
#include <btl/fmap.h>
#include <btl/collection.h>

namespace reactive
{
    using ObbMap = std::function<
        std::vector<avg::Obb>(ase::Vector2f size,
                std::vector<SizeHint> const&)>;

    template <typename T>
    struct IsObbMap :
        btl::All<
            std::is_assignable<ObbMap, T>,
            std::is_copy_constructible<T>
        > {};

    using SizeHintMap = std::function<
        SizeHint(std::vector<SizeHint> const&)
        >;

    template <typename T>
    struct IsSizeHintMap :
        btl::All<
            std::is_assignable<SizeHintMap, T>,
            std::is_copy_constructible<T>
        > {};

    template <typename T, typename = void>
    struct IsFactoryCollection : std::false_type {};

    template <typename T>
    struct IsFactoryCollection<T, std::enable_if_t
    <
        std::is_same
        <
            typename signal::DataBindType<T>::type, WidgetFactory
        >::value
    >> : std::true_type {};

    inline auto pickTransform(std::vector<avg::Obb> const& obbs, size_t index)
        -> avg::Transform
    {
        //assert(obbs.size() > index);
        /*if (obbs.size() <= index)
            return avg::Transform();*/
        return obbs.at(index).getTransform();
    }

    inline auto pickSize(std::vector<avg::Obb> const& obbs, size_t index)
        -> avg::Vector2f
    {
        //assert(obbs.size() > index);
        /*if (obbs.size() <= index)
            return avg::Vector2f();*/
        return obbs.at(index).getSize();
    }

    template <typename TCollection, typename TObbMap, typename TSizeHintMap,
             typename = std::enable_if_t
        <
            btl::All<
                IsFactoryCollection<TCollection>,
                IsSignal<TCollection>,
                IsObbMap<TObbMap>,
                IsSizeHintMap<TSizeHintMap>
            >::value
        >>
    auto layout(TSizeHintMap&& sizeHintMap, TObbMap&& obbMap,
            TCollection&& factories)
        -> WidgetFactory
    {
        auto hintsSignal = signal::share(signal::mbind(
                    [](std::vector<WidgetFactory> const& factories)
                    {
                        auto hints = btl::fmap(factories, [](auto const& f)
                        {
                            return f.getSizeHint();
                        });

                        return signal::combine(std::move(hints));
                    },
                    factories.clone()));

        auto widgetMap = [
            hintsSignal,
            factories=cloneOnCopy(btl::clone(factories)),
            obbMap=std::forward<TObbMap>(obbMap)
            ]
            (auto w)
            // -> Widget
        {
            auto obbs = signal::share(
                        signal::map(obbMap, w.getSize(), hintsSignal));

            auto delegate = [obbs](
                    Signal<btl::option<WidgetFactory>> factory,
                    Signal<size_t> index) mutable
                 -> Signal<btl::option<Widget>>
                {
                    auto t = signal::map(pickTransform, obbs, index.clone());
                    auto size = signal::map(pickSize, obbs, index.clone());

                    auto widget = signal::map(
                            [
                            t=btl::cloneOnCopy(std::move(t)),
                            size=btl::cloneOnCopy(std::move(size))
                            ]
                            (btl::option<WidgetFactory> factory)
                        -> btl::option<Widget>
                        {
                            if (!factory.valid())
                                return btl::none;

                            auto f = factory->clone()
                                | transform(t->clone())
                                ;

                            return btl::just(Widget(std::move(f)(size->clone())));
                        }, std::move(factory));

                    return widget;
                };

            auto widgets = signal::dataBind(factories->clone(), std::move(delegate));

            return std::move(w)
                | addWidgets(std::move(widgets));
        };

        return makeWidgetFactory()
            | mapWidget(std::move(widgetMap))
            | setSizeHint(
                    signal::map(
                        std::forward<TSizeHintMap>(sizeHintMap),
                        std::move(hintsSignal)
                        )
                    );
    }

    template <typename TFactories, typename TObbMap, typename TSizeHintMap,
             typename =
        std::enable_if_t<
            btl::All<
                IsObbMap<TObbMap>,
                IsSizeHintMap<TSizeHintMap>,
                btl::IsSequence<TFactories>
            >::value
        >
    >
    auto layout(TSizeHintMap sizeHintMap, TObbMap obbMap,
            TFactories factories)
        //-> WidgetFactory
    {
        auto hints = btl::fmap(factories, [](auto const& factory)
                {
                    return factory.getSizeHint();
                });

        auto hintsSignal = share(
                signal::combine(std::move(hints))
                );

        auto widgetMap = [obbMap=std::move(obbMap),
             hintsSignal, factories=btl::cloneOnCopy(std::move(factories))]
                 (auto w)
            // -> Widget
        {
            auto obbs = share(
                        signal::map(obbMap, w.getSize(), hintsSignal));

            //std::vector<Widget> widgets;
            //widgets.reserve(factories->size());
            size_t index = 0;
            auto widgets = btl::fmap(*factories, [&index, &obbs](auto&& f)
            //for (auto&& f : *factories)
            {
                auto t = signal::map([index](
                            std::vector<avg::Obb> const& obbs)
                        {
                            return obbs.at(index).getTransform();
                        }, obbs);

                auto size = signal::map([index](
                            std::vector<avg::Obb> const& obbs)
                        {
                            return obbs.at(index).getSize();
                        }, obbs);

                auto factory = f.clone()
                    | transform(std::move(t));

                ++index;

                return std::move(factory)(std::move(size));
            });

            return std::move(w)
                | addWidgets(std::move(widgets));
        };

        return makeWidgetFactory()
            | mapWidget(std::move(widgetMap))
            | setSizeHint(signal::map(std::move(sizeHintMap), hintsSignal));
    }
} // reactive

