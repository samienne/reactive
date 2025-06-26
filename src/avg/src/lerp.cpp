#include "lerp.h"

namespace avg
{

float lerp(float a, float b, float t)
{
    return a + (b-a) * t;
}

Vector2f lerp(Vector2f a, Vector2f b, float t)
{
    return a + (b-a) * t;
}

Rect lerp(Rect a, Rect b, float t)
{
    return Rect(
            lerp(a.getBottomLeft(), b.getBottomLeft(), t),
            lerp(a.getSize(), b.getSize(), t)
        );
}

Transform lerp(Transform const a, Transform const& b, float t)
{
    return Transform(
            lerp(a.getTranslation(), b.getTranslation(), t),
            lerp(a.getScale(), b.getScale(), t),
            lerp(a.getRotation(), b.getRotation(), t)
            );
}

Obb lerp(Obb const& a, Obb const& b, float t)
{
    return Obb(
            lerp(a.getSize(), b.getSize(), t),
            lerp(a.getTransform(), b.getTransform(), t)
            );
}

Color lerp(Color const& a, Color const& b, float t)
{
    return Color(
            lerp(a.getRed(), b.getRed(), t),
            lerp(a.getGreen(), b.getGreen(), t),
            lerp(a.getBlue(), b.getBlue(), t),
            lerp(a.getAlpha(), b.getAlpha(), t)
            );
}

Brush lerp(Brush const& a, Brush const& b, float t)
{
    return Brush(lerp(a.getColor(), b.getColor(), t));
}

Pen lerp(Pen const& a, Pen const& b, float t)
{
    return Pen(
            lerp(a.getBrush(), b.getBrush(), t),
            lerp(a.getWidth(), b.getWidth(), t),
            b.getJoinType(),
            b.getEndType()
            );
}

class CurveLerp : public CurveBase
{
public:
    CurveLerp(Curve a, Curve b, float t) :
        a_(std::move(a)),
        b_(std::move(b)),
        t_(t)
    {
    }

    float call(float t) const override
    {
        return (1.0f - t_) * a_(t) + t_ * b_(t);
    }

    std::type_info const& getTypeInfo() const override
    {
        return typeid(CurveLerp);
    }

    bool compare(CurveBase const& rhs) const override
    {
        auto const& r =  reinterpret_cast<CurveLerp const&>(rhs);

        return a_ == r.a_ && b_ == r.b_ && t_ == r.t_;
    }

private:
    Curve a_;
    Curve b_;
    float t_;
};

Curve lerp(Curve a, Curve b, float t)
{
    return Curve(std::make_shared<CurveLerp>(std::move(a), std::move(b), t));
}
} // namespace avg

