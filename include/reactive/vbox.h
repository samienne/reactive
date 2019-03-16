#pragma once

#include "box.h"
#include "widgetfactory.h"

#include <btl/collection.h>

#include <vector>

namespace reactive
{
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

