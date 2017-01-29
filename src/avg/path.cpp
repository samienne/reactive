#include "path.h"

#include "region.h"
#include "simplepolygon.h"
#include "transform.h"
#include "debug.h"

#include <ase/buffer.h>

#include <cmath>

namespace avg
{

class PathDeferred
{
public:
    ase::Vector2i toIVec(ase::Vector2f v, ase::Vector2f pixelSize,
            size_t resPerPixel) const;
    std::vector<SimplePolygon> toSimplePolygons(Transform const& transform,
            ase::Vector2f pixelSize, size_t resPerPixel) const;

    // CONIC
    float conicDerivative(float p1, float p2, float p3, float t) const;
    ase::Vector2f conicDerivative(ase::Vector2f p1, ase::Vector2f p2,
            ase::Vector2f p3, float t) const;

    float conicValue(float p1, float p2, float p3, float t) const;
    ase::Vector2f conicValue(ase::Vector2f p1, ase::Vector2f p2,
            ase::Vector2f p3, float t) const;
    float getNextConicT(ase::Vector2f p1, ase::Vector2f p2,
            ase::Vector2f p3, float t, ase::Vector2f pixelSize) const;

    // CUBIC
    float cubicDerivative(float p1, float p2, float p3, float p4,
            float t) const;
    ase::Vector2f cubicDerivative(ase::Vector2f p1, ase::Vector2f p2,
            ase::Vector2f p3, ase::Vector2f p4, float t) const;

    float cubicValue(float p1, float p2, float p3, float p4, float t) const;
    ase::Vector2f cubicValue(ase::Vector2f p1, ase::Vector2f p2,
            ase::Vector2f p3, ase::Vector2f p4, float t) const;
    float getNextCubicT(ase::Vector2f p1, ase::Vector2f p2,
            ase::Vector2f p3, ase::Vector2f p4, float t,
            ase::Vector2f pixelSize) const;

public:
    std::vector<Path::SegmentType> segments_;
    std::vector<ase::Vector2f> vertices_;
};

ase::Vector2i PathDeferred::toIVec(ase::Vector2f v, ase::Vector2f pixelSize,
        size_t resPerPixel) const
{
    float xRes = (float)resPerPixel / pixelSize[0];
    float yRes = (float)resPerPixel / pixelSize[1];

    return ase::Vector2i((int)(xRes * v[0]), (int)(yRes * v[1]));
}

float PathDeferred::conicDerivative(float p1,
        float p2, float p3, float t) const
{
    // D[(1-t)^2 * p_1 + 2 * (1-t) * t * p_2 + t^2 * p_3,t]
    // 2 p_1 t  -  4 p_2 t  +  2 p_3 t  -  2 p_1  +  2 p_2
    return 2.0f*t*p1 - 4.0f*t*p2 + 2.0f*t*p3 - 2.0f * p1 + 2.0f*p2;
}

ase::Vector2f PathDeferred::conicDerivative(ase::Vector2f p1, ase::Vector2f p2,
        ase::Vector2f p3, float t) const
{
    return ase::Vector2f(conicDerivative(p1[0], p2[0], p3[0], t),
            conicDerivative(p1[1], p2[1], p3[1], t));
}

float PathDeferred::conicValue(float p1, float p2, float p3, float t) const
{
    // (1-t)^2 * p_1 + 2 * (1-t) * t * p_2 + t^2 * p_3
    return (1.0f-t)*(1.0f-t)*p1 + 2.0f*(1.0f-t)*t*p2 + t*t*p3;
}

ase::Vector2f PathDeferred::conicValue(ase::Vector2f p1, ase::Vector2f p2,
        ase::Vector2f p3, float t) const
{
    return ase::Vector2f(conicValue(p1[0], p2[0], p3[0], t),
            conicValue(p1[1], p2[1], p3[1], t));
}

float PathDeferred::getNextConicT(ase::Vector2f p1, ase::Vector2f p2,
        ase::Vector2f p3, float t, ase::Vector2f pixelSize) const

{
    ase::Vector2f d = conicDerivative(p1, p2, p3, t);
    bool onX = std::abs(d[0]) < std::abs(d[1]);
    float dt;

    if (onX)
        dt = pixelSize[1] / std::abs(d[1]);
    else
        dt = pixelSize[0] / std::abs(d[0]);

    return t + 2.0f * dt;
}

float PathDeferred::cubicDerivative(float p1, float p2, float p3, float p4,
        float t) const
{
    // D[(1-t)^3 *p_1 + 3 * (1-t)^2 * t * p_2 + 3*(1-t)*t^2 * p_3 + t^3*p_4,t]
    //-3 p_1 t^2  +  9 p_2 t^2  -  9 p_3 t^2  +  3 p_4 t^2  +  6 p_1 t
    //-  12 p_2 t  +  6 p_3 t  -  3 p_1  +  3 p_2
    return -3.0f*p1*t*t + 9.0f*p2*t*t - 9.0f*p3*t*t + 3.0f*p4*t*t + 6.0f*p1*t
        - 12.0*p2*t + 6.0f*p3*t - 3.0f*p1 + 3.0f*p2;
}

ase::Vector2f PathDeferred::cubicDerivative(ase::Vector2f p1, ase::Vector2f p2,
        ase::Vector2f p3, ase::Vector2f p4, float t) const
{
    return ase::Vector2f(cubicDerivative(p1[0], p2[0], p3[0], p4[0], t),
            cubicDerivative(p1[1], p2[1], p3[1], p4[1], t));
}

float PathDeferred::cubicValue(float p1, float p2, float p3, float p4,
        float t) const
{
    // (1-t)^3 *p_1 + 3 * (1-t)^2 * t * p_2 + 3*(1-t)*t^2 * p_3 + t^3*p_4
    return (1.0f-t)*(1.0f-t)*(1.0f-t)*p1 + 3.0f*(1.0f-t)*(1.0f-t)*t*p2
        + 3.0f*(1.0f-t)*t*t*p3 + t*t*t*p4;
}

ase::Vector2f PathDeferred::cubicValue(ase::Vector2f p1, ase::Vector2f p2,
        ase::Vector2f p3, ase::Vector2f p4, float t) const
{
    return ase::Vector2f(cubicValue(p1[0], p2[0], p3[0], p4[0], t),
            cubicValue(p1[1], p2[1], p3[1], p4[1], t));
}

float PathDeferred::getNextCubicT(ase::Vector2f p1, ase::Vector2f p2,
        ase::Vector2f p3, ase::Vector2f p4, float t,
        ase::Vector2f pixelSize) const

{
    ase::Vector2f d = cubicDerivative(p1, p2, p3, p4, t);
    bool onY = std::abs(d[0]) < std::abs(d[1]);
    float dt;

    if (onY)
        dt = pixelSize[1] / std::abs(d[1]);
    else
        dt = pixelSize[0] / std::abs(d[0]);

    return t + 2.0 * dt;
}

std::vector<SimplePolygon> PathDeferred::toSimplePolygons(
        Transform const& transform, ase::Vector2f pixelSize,
        size_t resPerPixel) const

{
    size_t segment = 0;
    size_t vertex = 0;
    ase::Vector2f cur(0.0f, 0.0f);
    std::vector<SimplePolygon> polygons;
    std::vector<ase::Vector2i> vertices;

    while (segment < segments_.size())
    {
        ase::Vector2f p1;
        ase::Vector2f p2;
        ase::Vector2f p3;
        ase::Vector2f p4;
        float t;

        switch (segments_[segment])
        {
            case PathSpec::SEGMENT_START:
                if (!vertices.empty())
                    polygons.push_back(std::move(vertices));

                cur = transform * vertices_.at(vertex++);
                vertices.push_back(toIVec(cur, pixelSize, resPerPixel));
                break;

            case PathSpec::SEGMENT_LINE:
                cur = transform * vertices_.at(vertex++);
                vertices.push_back(toIVec(cur, pixelSize, resPerPixel));
                break;

            case PathSpec::SEGMENT_CONIC:
                p1 = cur;
                p2 = transform * vertices_.at(vertex++);
                p3 = transform * vertices_.at(vertex++);
                cur = p3;
                t = 0.0f;
                while (t < 1.0f)
                {
                    vertices.push_back(toIVec(conicValue(p1, p2, p3, t),
                                pixelSize, resPerPixel));
                    t = getNextConicT(p1, p2, p3, t, pixelSize);
                }
                vertices.push_back(toIVec(p3, pixelSize, resPerPixel));

                break;

            case PathSpec::SEGMENT_CUBIC:
                p1 = cur;
                p2 = transform * vertices_.at(vertex++);
                p3 = transform * vertices_.at(vertex++);
                p4 = transform * vertices_.at(vertex++);
                cur = p4;
                t = 0.0f;
                while (t < 1.0f)
                {
                    vertices.push_back(toIVec(cubicValue(p1, p2, p3, p4, t),
                                pixelSize, resPerPixel));
                    t = getNextCubicT(p1, p2, p3, p4, t, pixelSize);
                }
                vertices.push_back(toIVec(p4, pixelSize, resPerPixel));

                break;
        }

        ++segment;
    }

    if (!vertices.empty())
        polygons.push_back(std::move(vertices));

    return polygons;
}

Path::Path()
{
}

Path::Path(PathSpec&& pathSpec) :
    deferred_(std::make_shared<PathDeferred>())
{
    d()->segments_ = std::move(pathSpec.segments_);
    d()->vertices_ = std::move(pathSpec.vertices_);
}

Path::Path(PathSpec const& pathSpec) :
    Path(PathSpec(pathSpec))
{
}

Path::~Path()
{
}

bool Path::operator==(Path const& rhs) const
{
    if (d() == rhs.d())
        return transform_ == rhs.transform_;

    // Xor
    if ((d() && !rhs.d()) || (!d() && rhs.d()))
        return false;

    if (d()->segments_.size() != rhs.d()->segments_.size()
            || d()->vertices_.size() != rhs.d()->vertices_.size())
        return false;

    auto j = rhs.d()->segments_.begin();
    for (auto i = d()->segments_.begin(); i != d()->segments_.end(); ++i)
    {
        if (*i != *j)
            return false;

        ++j;
    }

    Transform combined = transform_.inverse() * rhs.transform_;

    auto k = rhs.d()->vertices_.begin();
    for (auto i = d()->vertices_.begin(); i != d()->vertices_.end(); ++i)
    {
        Vector2f v = *i - (combined * (*k));
        if (std::abs(v[0]) > 0.0001f || std::abs(v[1]) > 0.0001f)
            return false;
        ++k;
    }

    return true;
}

bool Path::operator!=(Path const& rhs) const
{
    return !(*this == rhs);
}

Path Path::operator+(Path const& rhs) const
{
    if (!d())
        return rhs;
    else if (!rhs.d())
        return *this;

    std::vector<SegmentType> segments;
    std::vector<ase::Vector2f> vertices;

    segments.reserve(d()->segments_.size() + rhs.d()->segments_.size());
    vertices.reserve(d()->vertices_.size() + rhs.d()->vertices_.size());

    for (auto i = d()->segments_.begin(); i != d()->segments_.end(); ++i)
        segments.push_back(*i);

    for (auto i = rhs.d()->segments_.begin(); i != rhs.d()->segments_.end();
            ++i)
        segments.push_back(*i);

    for (auto i = d()->vertices_.begin(); i != d()->vertices_.end(); ++i)
        vertices.push_back(*i);

    Transform combined = transform_.inverse() * rhs.transform_;
    for (auto i = rhs.d()->vertices_.begin(); i != rhs.d()->vertices_.end();
            ++i)
        vertices.push_back(combined * (*i));

    Path p = transform_ * Path(std::move(segments), std::move(vertices));
    return p;
}

Path Path::operator+(ase::Vector2f delta) const
{
    if (!d())
        return Path();

    Path p(*this);
    p.transform_ = std::move(p.transform_).translate(delta);
    return p;
}

Path Path::operator*(float scale) const
{
    if (!d())
        return Path();

    Path p(*this);
    p.transform_ = std::move(p.transform_).scale(scale);
    return p;
}

Path& Path::operator+=(ase::Vector2f delta)
{
    if (!d() || d()->segments_.empty())
        return *this;

    transform_ = std::move(transform_).translate(delta);

    return *this;
}

Path& Path::operator*=(float scale)
{
    if (!d() || d()->segments_.empty())
        return *this;

    transform_ = std::move(transform_).scale(scale);

    return *this;
}

Path& Path::operator+=(Path const& rhs)
{
    *this = *this + rhs;

    return *this;
}

bool Path::isEmpty() const
{
    return !d() || d()->segments_.empty();
}

Region Path::fillRegion(FillRule rule, ase::Vector2f pixelSize,
        size_t resPerPixel) const
{
    std::vector<SimplePolygon> polygons = d()->toSimplePolygons(transform_,
            pixelSize, resPerPixel);

    return avg::Region(std::move(polygons), rule, pixelSize, resPerPixel);
}

Region Path::offsetRegion(JoinType join, EndType end, float width,
        ase::Vector2f pixelSize, size_t resPerPixel) const
{
    std::vector<SimplePolygon> polygons = d()->toSimplePolygons(transform_,
            pixelSize, resPerPixel);

    return avg::Region(std::move(polygons), join, end, width, pixelSize,
            resPerPixel);
}

Path::Path(std::vector<SegmentType>&& segments,
        std::vector<ase::Vector2f>&& vertices) :
    deferred_(std::make_shared<PathDeferred>())
{
    d()->segments_ = std::move(segments);
    d()->vertices_ = std::move(vertices);
}

void Path::ensureUniqueness()
{
    if (deferred_.unique())
        return;

    if (d())
        deferred_ = std::make_shared<PathDeferred>(*d());
    else
        deferred_ = std::make_shared<PathDeferred>();
}

std::vector<Path::SegmentType> const& Path::getSegments() const
{
    if (!d())
    {
        static const std::vector<Path::SegmentType> empty;
        return empty;
    }
    return d()->segments_;
}

std::vector<ase::Vector2f> const& Path::getVertices() const
{
    if (!d())
    {
        static const std::vector<ase::Vector2f> empty;
        return empty;
    }
    return d()->vertices_;
}

//} // namespace

avg::Path operator*(const avg::Transform& t, const avg::Path& p)
{
    avg::Path result(p);
    result.transform_ = t * p.transform_;
    return result;
}

/*avg::Path operator*(ase::Matrix3f const& m, avg::Path const& p)
{
    std::vector<ase::Vector2f> vertices;
    vertices.reserve(p.d()->vertices_.size());
    for (auto i = p.d()->vertices_.begin(); i != p.d()->vertices_.end(); ++i)
    {
        ase::Vector3f v((*i)[0], (*i)[1], 1.0);
        v = m * v;
        ase::Vector2f s(v[0], v[1]);
        vertices.push_back(s);
    }

    return avg::Path(std::vector<Path::SegmentType>(p.d()->segments_),
            std::move(vertices));
}*/

std::ostream& operator<<(std::ostream& stream, const avg::Path& p)
{
    size_t segment = 0;
    size_t vertex = 0;
    ase::Vector2f cur(0.0f, 0.0f);
    std::vector<ase::Vector2i> vertices;

    while (segment < p.d()->segments_.size())
    {
        ase::Vector2f p1;
        ase::Vector2f p2;
        ase::Vector2f p3;
        ase::Vector2f p4;

        switch (p.d()->segments_[segment])
        {
            case PathSpec::SEGMENT_START:
                p1 = p.transform_ * p.d()->vertices_.at(vertex++);
                cur = p1;
                stream << "start(" << p1 << ")" << std::endl;
                break;

            case PathSpec::SEGMENT_LINE:
                p1 = p.transform_ * p.d()->vertices_.at(vertex++);
                cur = p1;
                stream << "lineTo(" << p1 << ")" << std::endl;
                break;

            case PathSpec::SEGMENT_CONIC:
                p1 = cur;
                p2 = p.transform_ * p.d()->vertices_.at(vertex++);
                p3 = p.transform_ * p.d()->vertices_.at(vertex++);
                cur = p3;

                stream << "conicTo(" << p2 << ", " << p3 << ")" << std::endl;
                break;

            case PathSpec::SEGMENT_CUBIC:
                p1 = cur;
                p2 = p.transform_ * p.d()->vertices_.at(vertex++);
                p3 = p.transform_ * p.d()->vertices_.at(vertex++);
                p4 = p.transform_ * p.d()->vertices_.at(vertex++);
                cur = p4;

                stream << "cubicTo(" << p2 << ", " << p3 << ", " << p4
                    << ")" << std::endl;

                break;
        }

        ++segment;
    }

    return stream;
}

}

