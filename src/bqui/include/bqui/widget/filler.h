#pragma once

#include "bqui/modifier/setsizehint.h"

#include "bqui/widget/widget.h"

#include "bqui/simplesizehint.h"

namespace bqui::widget
{
    inline auto hwfiller() -> AnyWidget
    {
        return makeWidget()
            | modifier::setSizeHint(bq::signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 100000}})));
    }

    inline auto hfiller() -> widget::AnyWidget
    {
        return makeWidget()
            | modifier::setSizeHint(bq::signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 0}})));
    }

    inline auto vfiller() -> widget::AnyWidget
    {
        return makeWidget()
            | modifier::setSizeHint(bq::signal::constant(
                        simpleSizeHint({{0, 0, 0}}, {{0, 0, 100000}})));
    }
} // namespace bqui::widget

