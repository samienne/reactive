#include "pointermoveevent.h"

namespace reactive
{

PointerMoveEvent transformPointerMoveEvent(
        PointerMoveEvent const& e,
        avg::Transform const& t
        )
{
    avg::Matrix2f rs = t.getRsMatrix();
    avg::Vector2f off = t.getTranslation();

    ase::Vector2f old = off + rs * (e.pos - e.rel);
    ase::Vector2f next = off + rs * e.pos;

    return
    {
        next,
        next - old,
        e.buttons,
        e.hover
    };
}

} // reactive

