#include "rect.h"

namespace avg
{

Rect::Rect()
{
}

Rect::Rect(ase::Vector2f bottomLeft, ase::Vector2f size) :
    bottomLeft_(bottomLeft),
    size_(size)
{
}

Rect::~Rect()
{
}

ase::Vector2f Rect::getBottomLeft() const
{
    return bottomLeft_;
}

ase::Vector2f Rect::getTopRight() const
{
    return bottomLeft_ + size_;
}

ase::Vector2f Rect::getSize() const
{
    return size_;
}

ase::Vector2f Rect::getCenter() const
{
    return ase::Vector2f(
            bottomLeft_[0] + size_[0] / 2.0f,
            bottomLeft_[1] + size_[1] / 2.0f);
}

float Rect::getLeft() const
{
    return bottomLeft_[0];
}

float Rect::getRight() const
{
    return bottomLeft_[0] + size_[0];
}

float Rect::getTop() const
{
    return bottomLeft_[1] + size_[1];
}

float Rect::getBottom() const
{
    return bottomLeft_[1];
}

bool Rect::isEmpty() const
{
    return size_[0] < 0.0f || size_[1] < 0.0f;
}

bool Rect::contains(ase::Vector2f pos) const
{
    if (isEmpty())
        return false;

    return pos[0] >= getLeft() && pos[0] <= getRight()
        && pos[1] >= getBottom() && pos[1] <= getTop();
}

} // namespace

