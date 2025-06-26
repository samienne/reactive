#include "bqui/clickevent.h"

namespace bqui
{

ClickEvent::ClickEvent(unsigned int pointer, unsigned int button,
    ase::Vector2f pos) :
    pos_(pos),
    pointer_(pointer),
    button_(button)
{
}

unsigned int ClickEvent::getPointer() const
{
    return pointer_;
}

unsigned int ClickEvent::getButton() const
{
    return button_;
}

ase::Vector2f ClickEvent::getPos() const
{
    return pos_;
}

ClickEvent ClickEvent::transform(avg::Transform const& t) const
{
    ClickEvent e(*this);
    e.pos_ = t.getTranslation() + t.getRsMatrix() * e.pos_;
    return e;
}

std::ostream& operator<<(std::ostream& stream,
        ClickEvent const& e)
{
    return stream << "ClickEvent{pos{" << e.pos_[0] << "," << e.pos_[1]
        << "},pointer:" << e.pointer_ << ",button:" << e.button_ << "}";
}

}

