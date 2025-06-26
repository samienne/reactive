#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <btl/function.h>

namespace bqui::widget
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

    BQUI_EXPORT AnyWidget layout(SizeHintMap sizeHintMap,
            ObbMap obbMap, std::vector<AnyWidget> widgets);
}

