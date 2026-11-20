#pragma once

#include "asevisibility.h"

namespace ase
{
    /**
     * @brief A standard pointer shape provided by the system.
     *
     * `hidden` shows no pointer at all.
     */
    enum class CursorShape
    {
        arrow,
        ibeam,
        hand,
        crosshair,
        move,
        resizeHorizontal,
        resizeVertical,
        resizeNwSe,
        resizeNeSw,
        notAllowed,
        wait,
        progress,
        hidden
    };

    /**
     * @brief The pointer a window shows over its content.
     *
     * Made from a shape, implicitly: `window.setCursor(CursorShape::ibeam)`.
     * A default-constructed cursor is the arrow.
     */
    class ASE_EXPORT Cursor
    {
    public:
        Cursor(CursorShape shape = CursorShape::arrow);

        CursorShape getShape() const;

        bool operator==(Cursor const& rhs) const;
        bool operator!=(Cursor const& rhs) const;

    private:
        CursorShape shape_;
    };
}
