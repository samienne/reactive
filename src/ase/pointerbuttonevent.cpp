#include "pointerbuttonevent.h"

namespace ase
{

PointerButtonEvent::PointerButtonEvent(unsigned int pointer,
        unsigned int button, ButtonState state, Vector2f pos) :
    pos_(pos),
    pointer_(pointer),
    button_(button),
    state_(state)
{
}

unsigned int PointerButtonEvent::getPointer() const
{
    return pointer_;
}

unsigned int PointerButtonEvent::getButton() const
{
    return button_;
}

ButtonState PointerButtonEvent::getState() const
{
    return state_;
}

Vector2f PointerButtonEvent::getPos() const
{
    return pos_;
}

PointerButtonEvent PointerButtonEvent::transform(avg::Transform const& t) const
{
    PointerButtonEvent e(*this);
    e.pos_ = t * e.pos_;
    return std::move(e);
}

std::ostream& operator<<(std::ostream& stream, PointerButtonEvent const& e)
{
    return stream
        << "Event:{"
        << "button:" << e.button_ << ", "
        << "state: " << (e.state_ == ButtonState::down ? "down" : "up") << ", "
        << "pos:{" << e.pos_[0] << "," << e.pos_[1] << "}"
        << "}";
}

} // namespace

