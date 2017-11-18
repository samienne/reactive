#pragma once

#include "box.h"
#include "widgetfactory.h"

#include <btl/collection.h>

#include <vector>

namespace reactive
{
    template <typename TCollection, typename = typename std::enable_if
        <
            IsFactoryCollection<TCollection>::value
        >::type>
    inline auto hbox(TCollection& factories)
        //-> WidgetFactory
    {
        return box<TCollection, Axis::x>(std::forward<TCollection>(factories));
    }

    inline auto hbox(std::initializer_list<WidgetFactory> factories)
        -> WidgetFactory
    {
        std::vector<WidgetFactory> fs;
        fs.reserve(factories.size());
        for (auto const& f : factories)
            fs.push_back(btl::clone(f));

        return box<Axis::x>(std::move(fs));
    }

    /*
    template <typename... Ts, typename = std::enable_if_t<
        btl::All<
            IsWidgetFactory<Ts>...
            >::value
        >
    >
    auto hbox(Ts... ts)
    {
        return box<Axis::x>(std::make_tuple(std::move(ts)...));
    }
    */
}

