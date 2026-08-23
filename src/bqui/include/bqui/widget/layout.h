#pragma once

#include "widget.h"

#include "bqui/bquivisibility.h"

#include <btl/function.h>

#include <vector>

namespace bqui::widget
{
    using SizeHintMap = btl::Function<
        SizeHint(std::vector<SizeHint> const&)
        >;

    template <typename T>
    struct IsSizeHintMap :
        btl::All<
            std::is_assignable<SizeHintMap, T>,
            std::is_copy_constructible<T>
        > {};
}
