#include "pointerbuttonevent.h"

namespace reactive
{

PointerButtonEvent transformPointerButtonEvent(
            PointerButtonEvent const& event,
            avg::Transform const& transform
            )
{
    return {
        event.pointer,
        event.button,
        event.state,
        transform * event.pos
    };
}

} // reactive

