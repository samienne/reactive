#pragma once

#include "widget/setsizehint.h"
#include "widget/builder.h"

#include "simplesizehint.h"
#include "sizehint.h"

namespace reactive
{
    inline auto hwfiller() -> widget::AnyBuilder
    {
        return widget::makeBuilder()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 100000}})));
    }

    inline auto hfiller() -> widget::AnyBuilder
    {
        return widget::makeBuilder()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 0}})));
    }

    inline auto vfiller() -> widget::AnyBuilder
    {
        return widget::makeBuilder()
            | widget::setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 0}}, {{0, 0, 100000}})));
    }
}

