#pragma once

#include "layout.h"
#include "widgetfactory.h"
#include "sizehint.h"

#include <btl/collection.h>

namespace reactive
{
    template <typename TCollection, typename = typename std::enable_if
        <
            btl::IsCollectionType<TCollection, WidgetFactory>::value
        >::type>
    WidgetFactory stack(Signal<std::vector<WidgetFactory>> widgets)
    {
        auto obbMap = [](ase::Vector2f size,
                std::vector<SizeHint> const& hints)
            -> std::vector<avg::Obb>
        {
            std::vector<avg::Obb> obbs;
            obbs.reserve(hints.size());
            auto obb = avg::Obb(size);
            for (size_t i = 0; i < hints.size(); ++i)
                obbs.push_back(obb);

            return obbs;
        };

        return layout(stackHints, obbMap, std::forward<TCollection>(widgets));
    }

    inline auto stack(std::vector<WidgetFactory> factories) -> WidgetFactory
    {
        return stack(std::move(factories));
    }
}

