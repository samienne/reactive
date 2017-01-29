#pragma once

#include "box.h"
#include "widgetfactory.h"

#include "signaltype.h"

#include <btl/collection.h>

#include <vector>

namespace reactive
{
    template <typename TCollection, typename = typename
        std::enable_if<
            IsFactoryCollection<TCollection>::value
            >::type>
    auto vbox(TCollection&& factories)
    -> decltype(
            box<TCollection, Axis::y>(std::forward<TCollection>(factories))
            )
        //-> WidgetFactory
    {
        return box<TCollection, Axis::y>(std::forward<TCollection>(factories));
    }

    inline auto vbox(std::initializer_list<WidgetFactory> factories)
        -> WidgetFactory
    {
        std::vector<WidgetFactory> fs;
        fs.reserve(factories.size());
        for (auto const& f : factories)
            fs.push_back(btl::clone(f));

        return box<Axis::y>(std::move(fs));
    }
}

