#include "cursor.h"

namespace ase
{

Cursor::Cursor(CursorKind kind) :
    kind_(kind)
{
}

CursorKind Cursor::getKind() const
{
    return kind_;
}

bool Cursor::operator==(Cursor const& rhs) const
{
    return kind_ == rhs.kind_;
}

bool Cursor::operator!=(Cursor const& rhs) const
{
    return !(*this == rhs);
}

} // namespace ase
