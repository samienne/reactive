#include "rect.h"

namespace avg
{

Rect::Rect() :
    bottomLeft_(Vector2f(0.0f, 0.0f)),
    size_(Vector2f(-1.0f, -1.0f))
{
}

Rect::Rect(Vector2f bottomLeft, Vector2f size) :
    bottomLeft_(bottomLeft),
    size_(size)
{
}

Rect::~Rect()
{
}

Vector2f Rect::getBottomLeft() const
{
    return *bottomLeft_;
}

Vector2f Rect::getBottomRight() const
{
    return Vector2f(getRight(), getBottom());
}


Vector2f Rect::getTopRight() const
{
    return *bottomLeft_ + *size_;
}

Vector2f Rect::getTopLeft() const
{
    return Vector2f(getLeft(), getTop());
}

Vector2f Rect::getSize() const
{
    if (isEmpty())
        return Vector2f(0.0f, 0.0f);

    return *size_;
}

Vector2f Rect::getCenter() const
{
    return Vector2f(
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

float Rect::getWidth() const
{
    return size_[0];
}

float Rect::getHeight() const
{
    return size_[1];
}

bool Rect::isEmpty() const
{
    return size_[0] < 0.0f || size_[1] < 0.0f;
}

bool Rect::contains(Vector2f pos) const
{
    if (isEmpty())
        return false;

    return pos[0] >= getLeft() && pos[0] <= getRight()
        && pos[1] >= getBottom() && pos[1] <= getTop();
}

Rect Rect::include(Vector2f point) const
{
    if (isEmpty())
        return Rect(point, Vector2f(0.0f, 0.0f));

    float x1 = std::min(point.x(), getLeft());
    float x2 = std::max(point.x(), getRight());
    float y1 = std::min(point.y(), getBottom());
    float y2 = std::max(point.y(), getTop());

    return Rect(Vector2f(x1, y1), Vector2f(x2-x1, y2-y1));
}

} // avg

