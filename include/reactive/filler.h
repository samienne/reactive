#pragma once

#include "widget/setsizehint.h"
#include "widget/widget.h"

#include "simplesizehint.h"
#include "sizehint.h"

namespace reactive
{
    inline auto hwfiller() -> widget::AnyWidget
    {
        return widget::makeWidget()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 100000}})));
    }

    inline auto hfiller() -> widget::AnyWidget
    {
        return widget::makeWidget()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 0}})));
    }

    inline auto vfiller() -> widget::AnyWidget
    {
        return widget::makeWidget()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 0}}, {{0, 0, 100000}})));
    }
}

