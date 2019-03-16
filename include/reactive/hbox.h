#pragma once

#include "box.h"
#include "widgetfactory.h"

#include <btl/collection.h>

#include <vector>

namespace reactive
{
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

