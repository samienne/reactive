#pragma once

#include "widgetfactory.h"

#include "signal/map.h"
#include "signal/mbind.h"
#include "signal/combine.h"

#include "signal.h"

#include <btl/fmap.h>
#include <btl/function.h>

namespace reactive
{
    using ObbMap = btl::Function<
        std::vector<avg::Obb>(ase::Vector2f size,
                std::vector<SizeHint> const&)>;

    template <typename T>
    struct IsObbMap :
        btl::All<
            std::is_assignable<ObbMap, T>,
            std::is_copy_constructible<T>
        > {};

    using SizeHintMap = btl::Function<
        SizeHint(std::vector<SizeHint> const&)
        >;

    template <typename T>
    struct IsSizeHintMap :
        btl::All<
            std::is_assignable<SizeHintMap, T>,
            std::is_copy_constructible<T>
        > {};

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

#if 1
    BTL_VISIBLE WidgetFactory layout(SizeHintMap sizeHintMap, ObbMap obbMap,
            std::vector<WidgetFactory> factories);
#else
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
#endif
} // reactive

