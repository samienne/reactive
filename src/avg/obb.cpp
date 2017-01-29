#include "obb.h"

#include "rect.h"

namespace avg
{

Obb::Obb() :
    size_(0.0f, 0.0f)
{
}

Obb::Obb(Vector2f size) :
    size_(size)
{
}

Obb::Obb(Rect const& r) :
    transform_(Transform().translate(r.getBottomLeft())),
    size_(r.getSize())
{
}

bool Obb::contains(Vector2f p) const
{
    p = transform_.inverse() * p;

    return
        0.0f <= p[0] && p[0] < size_[0] &&
        0.0f <= p[1] && p[1] < size_[1];
}

Vector2f Obb::getSize() const
{
    return size_;
}

Vector2f Obb::getCenter() const
{
    return transform_ * Vector2f(size_[0] / 2.0f, size_[1] / 2.0f);
}

Transform const& Obb::getTransform() const
{
    return transform_;
}

Obb Obb::transformR(avg::Transform const& t) const
{
    avg::Obb obb(*this);
    obb.transform_ = obb.transform_ * t;
    return std::move(obb);
}

Obb Obb::setSize(Vector2f size) const
{
    avg::Obb obb(*this);
    obb.size_ = size;
    return std::move(obb);
}

Obb Obb::operator+(Obb const& obb) const
{
    auto t = transform_.inverse();
    auto t2 = obb.transform_;

    ase::Vector2f ps[] = {
        t * t2 * ase::Vector2f(0.0f, 0.0f),
        t * t2 * ase::Vector2f(obb.size_[0], 0.0f),
        t * t2 * ase::Vector2f(obb.size_),
        t * t2 * ase::Vector2f(0.0f, obb.size_[1])
    };

    float width = size_[0];
    float height = size_[1];

    for (int i = 0; i < 4; ++i)
    {
        width = std::max(width, ps[i][0]);
        height = std::max(height, ps[i][1]);
    }

    return transform_ * Obb(Vector2f(width, height));
}

bool Obb::operator==(Obb const& obb) const
{
    return transform_ == obb.transform_ && size_ == obb.size_;
}

bool Obb::operator!=(Obb const& obb) const
{
    return transform_ != obb.transform_ || size_ != obb.size_;
}

Obb operator*(avg::Transform const& t, Obb const& obb)
{
    Obb r(obb);
    r.transform_ = t * r.transform_;
    return r;
}

std::ostream& operator<<(std::ostream& stream, Obb const& obb)
{
    return stream << "Obb{s:{" << obb.size_[0] << ", " << obb.size_[1]
        << "}, t:" << obb.transform_ << "}";
}

} // namespace

