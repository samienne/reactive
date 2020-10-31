#include "path.h"

#include "obb.h"
#include "region.h"
#include "simplepolygon.h"
#include "transform.h"
#include "rect.h"
#include "calculatebounds.h"
#include "debug.h"

#include <ase/buffer.h>
#include <ase/matrix.h>

#include <pmr/shared_ptr.h>

#include <cmath>

namespace avg
{

class PathDeferred
{
public:
    PathDeferred(pmr::memory_resource* memory);

public:
    btl::Buffer data_;
    Rect controlBb_;
};

namespace
{

Vector2i toIVec(Vector2f v, Vector2f pixelSize,
        float resPerPixel)
{
    float xRes = resPerPixel / pixelSize[0];
    float yRes = resPerPixel / pixelSize[1];

    return Vector2i((int)(xRes * v[0]), (int)(yRes * v[1]));
}

float conicDerivative(float p1,
        float p2, float p3, float t)
{
    // D[(1-t)^2 * p_1 + 2 * (1-t) * t * p_2 + t^2 * p_3,t]
    // 2 p_1 t  -  4 p_2 t  +  2 p_3 t  -  2 p_1  +  2 p_2
    return 2.0f*t*p1 - 4.0f*t*p2 + 2.0f*t*p3 - 2.0f * p1 + 2.0f*p2;
}

Vector2f conicDerivative(Vector2f p1, Vector2f p2,
        Vector2f p3, float t)
{
    return Vector2f(conicDerivative(p1[0], p2[0], p3[0], t),
            conicDerivative(p1[1], p2[1], p3[1], t));
}

float conicValue(float p1, float p2, float p3, float t)
{
    // (1-t)^2 * p_1 + 2 * (1-t) * t * p_2 + t^2 * p_3
    return (1.0f-t)*(1.0f-t)*p1 + 2.0f*(1.0f-t)*t*p2 + t*t*p3;
}

Vector2f conicValue(Vector2f p1, Vector2f p2,
        Vector2f p3, float t)
{
    return Vector2f(conicValue(p1[0], p2[0], p3[0], t),
            conicValue(p1[1], p2[1], p3[1], t));
}

float getNextConicT(Vector2f p1, Vector2f p2,
        Vector2f p3, float t, Vector2f pixelSize)

{
    Vector2f d = conicDerivative(p1, p2, p3, t);
    bool onX = std::abs(d[0]) < std::abs(d[1]);
    float dt;

    if (onX)
        dt = pixelSize[1] / std::abs(d[1]);
    else
        dt = pixelSize[0] / std::abs(d[0]);

    return t + 2.0f * dt;
}

float cubicDerivative(float p1, float p2, float p3, float p4,
        float t)
{
    // D[(1-t)^3 *p_1 + 3 * (1-t)^2 * t * p_2 + 3*(1-t)*t^2 * p_3 + t^3*p_4,t]
    //-3 p_1 t^2  +  9 p_2 t^2  -  9 p_3 t^2  +  3 p_4 t^2  +  6 p_1 t
    //-  12 p_2 t  +  6 p_3 t  -  3 p_1  +  3 p_2
    return -3.0f*p1*t*t + 9.0f*p2*t*t - 9.0f*p3*t*t + 3.0f*p4*t*t + 6.0f*p1*t
        - 12.0*p2*t + 6.0f*p3*t - 3.0f*p1 + 3.0f*p2;
}

Vector2f cubicDerivative(Vector2f p1, Vector2f p2,
        Vector2f p3, Vector2f p4, float t)
{
    return Vector2f(cubicDerivative(p1[0], p2[0], p3[0], p4[0], t),
            cubicDerivative(p1[1], p2[1], p3[1], p4[1], t));
}

float cubicValue(float p1, float p2, float p3, float p4,
        float t)
{
    // (1-t)^3 *p_1 + 3 * (1-t)^2 * t * p_2 + 3*(1-t)*t^2 * p_3 + t^3*p_4
    return (1.0f-t)*(1.0f-t)*(1.0f-t)*p1 + 3.0f*(1.0f-t)*(1.0f-t)*t*p2
        + 3.0f*(1.0f-t)*t*t*p3 + t*t*t*p4;
}

Vector2f cubicValue(Vector2f p1, Vector2f p2,
        Vector2f p3, Vector2f p4, float t)
{
    return Vector2f(cubicValue(p1[0], p2[0], p3[0], p4[0], t),
            cubicValue(p1[1], p2[1], p3[1], p4[1], t));
}

float getNextCubicT(Vector2f p1, Vector2f p2,
        Vector2f p3, Vector2f p4, float t,
        Vector2f pixelSize)

{
    Vector2f d = cubicDerivative(p1, p2, p3, p4, t);
    bool onY = std::abs(d[0]) < std::abs(d[1]);
    float dt;

    if (onY)
        dt = pixelSize[1] / std::abs(d[1]);
    else
        dt = pixelSize[0] / std::abs(d[0]);

    return t + 2.0 * dt;
}

pmr::vector<SimplePolygon> toSimplePolygons(
        pmr::memory_resource* memory,
        Path const& path,
        Transform const& transform, Vector2f pixelSize,
        float resPerPixel)

{
    Vector2f cur(0.0f, 0.0f);
    pmr::vector<SimplePolygon> polygons(memory);
    pmr::vector<Vector2i> vertices(memory);

    auto rs = transform.getRsMatrix();
    auto offset = transform.getTranslation();

    for (auto i = path.begin(); i != path.end(); ++i)
    {
        Vector2f p1;
        Vector2f p2;
        Vector2f p3;
        Vector2f p4;
        float t;

        switch (i.getType())
        {
            case Path::SegmentType::start:
            {
                auto const& start = i.getStart();

                if (!vertices.empty())
                {
                    polygons.push_back(std::move(vertices));
                    vertices.clear();
                }

                cur = offset + rs * start.v;
                vertices.push_back(toIVec(cur, pixelSize, resPerPixel));
                break;
            }

            case Path::SegmentType::line:
            {
                auto const& line = i.getLine();

                cur = offset + rs * line.v;
                vertices.push_back(toIVec(cur, pixelSize, resPerPixel));
                break;
            }

            case Path::SegmentType::conic:
            {
                auto const& conic = i.getConic();

                p1 = cur;
                p2 = offset + rs * conic.v1;
                p3 = offset + rs * conic.v2;
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
            }

            case Path::SegmentType::cubic:
            {
                auto const& cubic = i.getCubic();

                p1 = cur;
                p2 = offset + rs * cubic.v1;
                p3 = offset + rs * cubic.v2;
                p4 = offset + rs * cubic.v3;
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

            case Path::SegmentType::arc:
            {
                auto const& arc = i.getArc();

                p1 = cur;
                Vector2f center = offset + rs * arc.center;
                float angle = arc.angle;

                Vector2f diff = p1-center;
                float radius = std::sqrt(diff[0]*diff[0]+diff[1]*diff[1]);

                int steps = static_cast<int>(std::ceil(angle * radius) / 2.0f);
                float step = angle / (float)steps;

                ase::Matrix2f rotation;
                rotation(0,0) = std::cos(step);
                rotation(0,1) = -std::sin(step);
                rotation(1,0) = std::sin(step);
                rotation(1,1) = std::cos(step);

                Vector2f cur = diff;

                for (int i = 0; i < steps; ++i)
                {
                    cur = rotation * cur;

                    vertices.push_back(toIVec(
                                center + cur,
                                pixelSize,
                                resPerPixel
                                ));
                }

                break;
            }
        } // switch
    } // for segments

    if (!vertices.empty())
        polygons.push_back(std::move(vertices));

    return polygons;
}


} // anonymous namespace

PathDeferred::PathDeferred(pmr::memory_resource* memory) :
    data_(memory)
{
}

Path::Path(pmr::memory_resource* memory) :
    memory_(memory)
{
}

Path::~Path()
{
}

pmr::memory_resource* Path::getResource() const
{
    return memory_;
}

bool Path::operator==(Path const& rhs) const
{
    if (d() == rhs.d())
        return transform_ == rhs.transform_;

    // Xor
    if ((d() && !rhs.d()) || (!d() && rhs.d()))
        return false;

    if (d()->data_.size() != rhs.d()->data_.size())
        return false;

    auto i = begin();
    auto j = rhs.begin();

    Transform combined = transform_.inverse() * rhs.transform_;
    Matrix2f combinedSr = combined.getRsMatrix();

    auto cmp = [&](Vector2f v1, Vector2f v2)
    {
        Vector2f v = v1 - combined.getTranslation() - (combinedSr * v2);
        return std::abs(v[0]) < 0.0001f && std::abs(v[1]) < 0.0001f;
    };

    while (i != end() && j != rhs.end())
    {
        if (i.getType() != j.getType())
            return false;

        switch (i.getType())
        {
            case SegmentType::start:
                if (!cmp(i.getStart().v, j.getStart().v))
                    return false;
                break;
            case SegmentType::line:
                if (!cmp(i.getLine().v, j.getLine().v))
                    return false;
                break;
            case SegmentType::conic:
                if (!cmp(i.getConic().v1, j.getConic().v1)
                        || !cmp(i.getConic().v2, j.getConic().v2))
                    return false;
                break;
            case SegmentType::cubic:
                if (!cmp(i.getCubic().v1, j.getCubic().v1)
                        || !cmp(i.getCubic().v2, j.getCubic().v2)
                        || !cmp(i.getCubic().v3, j.getCubic().v3))
                    return false;
                break;
            case SegmentType::arc:
                if (!cmp(i.getArc().center, j.getArc().center)
                        || i.getArc().angle != j.getArc().angle)
                    return false;
                break;
        }

        ++i;
        ++j;
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

    btl::Buffer data(d()->data_);
    data.reserve(data.size() + rhs.d()->data_.size());

    auto combined = transform_.inverse() * rhs.transform_;

    auto combinedRs = combined.getRsMatrix();
    auto combinedT = combined.getTranslation();

    auto transform = [&](Vector2f& v)
    {
        v = combinedT + combinedRs * v;
    };

    for (auto i = rhs.begin(); i != rhs.end(); ++i)
    {
        switch (i.getType())
        {
            case SegmentType::start:
            {
                auto start = i.getStart();
                transform(start.v);
                data.push(start);
                break;
            }
            case SegmentType::line:
            {
                auto line = i.getLine();
                transform(line.v);
                data.push(line);
                break;
            }
            case SegmentType::conic:
            {
                auto conic = i.getConic();
                transform(conic.v1);
                transform(conic.v2);
                data.push(conic);
                break;
            }
            case SegmentType::cubic:
            {
                auto cubic = i.getCubic();
                transform(cubic.v1);
                transform(cubic.v2);
                transform(cubic.v3);
                data.push(cubic);
                break;
            }
            case SegmentType::arc:
            {
                auto arc = i.getArc();
                transform(arc.center);
                data.push(arc);
                break;
            }
        }
    }

    return Path(std::move(data));
}

Path Path::operator+(Vector2f delta) const
{
    if (!d())
        return Path(memory_);

    Path p(*this);
    p.transform_ = std::move(p.transform_).translate(delta);
    return p;
}

Path Path::operator*(float scale) const
{
    if (!d())
        return Path(memory_);

    Path p(*this);
    p.transform_ = std::move(p.transform_).scale(scale);
    return p;
}

Path& Path::operator+=(Vector2f delta)
{
    if (!d() || d()->data_.empty())
        return *this;

    transform_ = std::move(transform_).translate(delta);

    return *this;
}

Path& Path::operator*=(float scale)
{
    if (!d() || d()->data_.empty())
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
    return !d() || d()->data_.empty();
}

Path::ConstIterator Path::begin() const
{
    if (!d())
        return ConstIterator(nullptr);

    return ConstIterator(d()->data_.data());
}

Path::ConstIterator Path::end() const
{
    if (!d())
        return ConstIterator(nullptr);

    return ConstIterator(reinterpret_cast<char const*>(d()->data_.data())
            + d()->data_.size());
}

Region Path::fillRegion(pmr::memory_resource* memory,
        FillRule rule, Vector2f pixelSize, float resPerPixel) const
{
    pmr::vector<SimplePolygon> polygons = toSimplePolygons(memory, *this,
            transform_, pixelSize, resPerPixel);

    return avg::Region(memory, std::move(polygons), rule, pixelSize,
            resPerPixel);
}

Region Path::offsetRegion(pmr::memory_resource* memory, JoinType join,
        EndType end, float width, Vector2f pixelSize, float resPerPixel) const
{
    pmr::vector<SimplePolygon> polygons = toSimplePolygons(memory, *this,
            transform_, pixelSize, resPerPixel);

    return avg::Region(memory, std::move(polygons), join, end, width,
            pixelSize, resPerPixel);
}

Rect Path::getControlBb() const
{
    if (isEmpty())
        return Rect();

    return getControlObb().getBoundingRect();
}

Obb Path::getControlObb() const
{
    if (!d())
        return Obb();

    return transform_ * Obb(d()->controlBb_);
}

Path::Path(btl::Buffer&& buffer) :
    memory_(buffer.resource()),
    deferred_(pmr::make_shared<PathDeferred>(memory_, memory_))
{
    d()->data_ = std::move(buffer);
    d()->controlBb_ = calculateBounds(*this);
}

void Path::ensureUniqueness()
{
    if (deferred_.unique())
        return;

    if (d())
        deferred_ = pmr::make_shared<PathDeferred>(memory_, *d());
    else
        deferred_ = pmr::make_shared<PathDeferred>(memory_, memory_);
}

avg::Path operator*(const avg::Transform& t, const avg::Path& p)
{
    avg::Path result(p);
    result.transform_ = t * p.transform_;
    return result;
}

std::ostream& operator<<(std::ostream& stream, const avg::Path& p)
{
    Vector2f cur(0.0f, 0.0f);
    std::vector<Vector2i> vertices;

    auto sr = p.transform_.getRsMatrix();
    auto off = p.transform_.getTranslation();

    for (auto i = p.begin(); i != p.end(); ++i)
    {
        Vector2f p1;
        Vector2f p2;
        Vector2f p3;
        Vector2f p4;

        switch (i.getType())
        {
            case Path::SegmentType::start:
                p1 = off + sr * i.getStart().v;
                cur = p1;
                stream << "start(" << p1 << ")" << std::endl;
                break;

            case Path::SegmentType::line:
                p1 = off + sr * i.getLine().v;
                cur = p1;
                stream << "lineTo(" << p1 << ")" << std::endl;
                break;

            case Path::SegmentType::conic:
                p1 = cur;
                p2 = off + sr * i.getConic().v1;
                p3 = off + sr * i.getConic().v2;
                cur = p3;

                stream << "conicTo(" << p2 << ", " << p3 << ")" << std::endl;
                break;

            case Path::SegmentType::cubic:
                p1 = cur;
                p2 = off + sr * i.getCubic().v1;
                p3 = off + sr * i.getCubic().v2;
                p4 = off + sr * i.getCubic().v3;
                cur = p4;

                stream << "cubicTo(" << p2 << ", " << p3 << ", " << p4
                    << ")" << std::endl;

                break;
            case Path::SegmentType::arc:
                p1 = cur;
                p2 = off + sr * i.getArc().center;

                stream << "arc(" <<
                    p2 << ", " << i.getArc().angle
                    << ")" << std::endl;
                break;
    }
    }

    return stream;
}

Path Path::with_resource(pmr::memory_resource* memory) const
{
    if (!d())
        return Path(memory);

    btl::Buffer buffer(d()->data_.with_resource(memory));

    Path result(std::move(buffer));
    result.transform_ = transform_;
    result.d()->controlBb_ = d()->controlBb_;

    return result;
}

} // namespace avg

