#include "scissorrect.h"

#include <tuple>

namespace ase
{

namespace
{
    auto key(ScissorRect const& r)
    {
        return std::tie(r.x, r.y, r.width, r.height);
    }
} // anonymous namespace

bool ScissorRect::operator==(ScissorRect const& rhs) const
{
    return key(*this) == key(rhs);
}

bool ScissorRect::operator!=(ScissorRect const& rhs) const
{
    return key(*this) != key(rhs);
}

bool ScissorRect::operator<(ScissorRect const& rhs) const
{
    return key(*this) < key(rhs);
}

} // namespace ase
