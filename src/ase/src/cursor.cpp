#include "cursor.h"

namespace ase
{

Cursor::Cursor(CursorShape shape) :
    shape_(shape)
{
}

CursorShape Cursor::getShape() const
{
    return shape_;
}

bool Cursor::operator==(Cursor const& rhs) const
{
    return shape_ == rhs.shape_;
}

bool Cursor::operator!=(Cursor const& rhs) const
{
    return !(*this == rhs);
}

} // namespace ase
