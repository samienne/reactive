#pragma once

#include "asevisibility.h"

namespace ase
{
    /**
     * @brief A standard pointer shape provided by the system.
     *
     * `hidden` shows no pointer at all.
     */
    enum class CursorKind
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
     * Made from a kind, implicitly: `window.setCursor(CursorKind::ibeam)`.
     * A default-constructed cursor is the arrow.
     */
    class ASE_EXPORT Cursor
    {
    public:
        Cursor(CursorKind kind = CursorKind::arrow);

        CursorKind getKind() const;

        bool operator==(Cursor const& rhs) const;
        bool operator!=(Cursor const& rhs) const;

    private:
        CursorKind kind_;
    };
}
