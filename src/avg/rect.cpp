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

bool Rect::operator==(Rect const& rhs) const
{
    if (isEmpty() == rhs.isEmpty())
        return true;

    return getLeft() == rhs.getLeft()
        && getRight() == rhs.getRight()
        && getBottom() == rhs.getBottom()
        && getTop() == rhs.getTop();
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

Rect Rect::scaled(float scale) const
{
    return Rect(scale * (*bottomLeft_), scale * (*size_));
}

Rect Rect::enlarged(float amount) const
{
    return Rect(
            Vector2f(getLeft() - amount, getBottom() - amount),
            Vector2f(amount * 2.0f, amount * 2.0f)
            );
}

Rect Rect::intersected(Rect const& rhs) const
{
    if (!overlaps(rhs))
        return Rect();

    float l = std::max(getLeft(), rhs.getLeft());
    float r = std::min(getRight(), rhs.getRight());
    float b = std::max(getBottom(), rhs.getBottom());
    float t = std::min(getTop(), rhs.getTop());

    return Rect(Vector2f(l, b), Vector2f(r-l, t-b));
}

Rect Rect::translated(Vector2f v) const
{
    return Rect(getBottomLeft() + v, getSize());
}

bool Rect::contains(Vector2f pos) const
{
    if (isEmpty())
        return false;

    return pos[0] >= getLeft() && pos[0] <= getRight()
        && pos[1] >= getBottom() && pos[1] <= getTop();
}

bool Rect::overlaps(Rect const& r) const
{
    if (isEmpty() || r.isEmpty())
        return false;

    return getLeft() < r.getRight() && getRight() > r.getLeft()
        && getTop() > r.getBottom() && getBottom() < r.getTop();
}

bool Rect::isFullyContainedIn(Rect const& r) const
{
    if (isEmpty())
        return true;
    else if (r.isEmpty())
        return false;

    return getLeft() >= r.getLeft()
        && getRight() <= r.getRight()
        && getBottom() >= r.getBottom()
        && getTop() <= r.getTop();
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

