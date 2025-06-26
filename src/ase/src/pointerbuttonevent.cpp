#include "pointerbuttonevent.h"

namespace ase
{

std::ostream& operator<<(std::ostream& stream, PointerButtonEvent const& e)
{
    return stream
        << "Event:{"
        << "button:" << e.button << ", "
        << "state: " << (e.state == ButtonState::down ? "down" : "up") << ", "
        << "pos:{" << e.pos[0] << "," << e.pos[1] << "}"
        << "}";
}

} // namespace

