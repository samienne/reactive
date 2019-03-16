#pragma once

#include "widgetfactory.h"
#include "simplesizehint.h"
#include "sizehint.h"

namespace reactive
{
    inline auto hwfiller() -> WidgetFactory
    {
        return makeWidgetFactory()
            | setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 100000}})));
    }

    inline auto hfiller() -> WidgetFactory
    {
        return makeWidgetFactory()
            | setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 100000}}, {{0, 0, 0}})));
    }

    inline auto vfiller() -> WidgetFactory
    {
        return makeWidgetFactory()
            | setSizeHint(signal::constant(
                        simpleSizeHint({{0, 0, 0}}, {{0, 0, 100000}})));
    }
}

