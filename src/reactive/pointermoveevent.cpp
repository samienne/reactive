#include "pointermoveevent.h"

namespace reactive
{

PointerMoveEvent transformPointerMoveEvent(
        PointerMoveEvent const& e,
        avg::Transform const& t
        )
{
    ase::Vector2f old = t * (e.pos - e.rel);
    ase::Vector2f next = t * e.pos;

    return
    {
        next,
        next - old,
        e.buttons,
        e.hover
    };
}

} // reactive

