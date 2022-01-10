#pragma once

#include "widget/builder.h"

#include "simplesizehint.h"
#include "sizehint.h"

namespace reactive
{
    inline auto hwfiller() -> widget::Builder
    {
        return widget::makeBuilder()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 100000}})));
    }

    inline auto hfiller() -> widget::Builder
    {
        return widget::makeBuilder()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 0}})));
    }

    inline auto vfiller() -> widget::Builder
    {
        return widget::makeBuilder()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 0}}, {{0, 0, 100000}})));
    }
}

