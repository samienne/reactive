#pragma once

#include "widgetfactory.h"

#include "signal/map.h"
#include "signal/mbind.h"
#include "signal/combine.h"

#include "signal.h"
#include "reactivevisibility.h"

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

    REACTIVE_EXPORT WidgetFactory layout(SizeHintMap sizeHintMap,
            ObbMap obbMap, std::vector<WidgetFactory> factories);
} // reactive

