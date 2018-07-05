#include "softmesh.h"

#include "brush.h"
#include "vector.h"

namespace
{
    /*
    struct Plane2d
    {
        float a;
        float b;
        float d;
    };

    struct LineSegment2d
    {
        avg::Vector2f p1;
        avg::Vector2f p2;
    };

    bool doesLineSegmentIntersectPlane(LineSegment2d const& l, Plane2d const& p)
    {
        float d1 = p.a * l.p1.x() + p.b * l.p1.y() - p.d;
        float d2 = p.a * l.p2.x() + p.b * l.p2.y() - p.d;

        return (d1 < 0.0f && 0.0f < d2)
            || (d2 < 0.0f && 0.0f < d1);
    }

    avg::Vector2f getLinePlaneIntersection(LineSegment2d const& l,
            Plane2d const& p)
    {
        avg::Vector2f lv = l.p2 - l.p1;

        avg::Vector2f n(p.a, p.b);
        avg::Vector2f p0(-p.a * p.d, -p.b * p.d); // point on the plane

        float s = -(p.a * p0.x() + p.b * p0.y() + p.d) / n.dot(lv);

        return { lv.x() * s, lv.y() * s};
    }

    std::vector<avg::SoftMesh::Vertex> clipPolygonByPlane(
            std::vector<avg::SoftMesh::Vertex> const& vertices,
            Plane2d const& plane)
    {
        if (vertices.size() < 3)
            return vertices;

        std::vector<avg::SoftMesh::Vertex> result;

        for (size_t i = 0; i < vertices.size() - 1; ++i)
        {
            avg::SoftMesh::Vertex const& v1 = vertices[i];
            avg::SoftMesh::Vertex const& v2 = vertices[i+1];
            LineSegment2d l{
                avg::Vector2f(v1[4], v1[5]),
                avg::Vector2f(v2[4], v2[5])
            };

            if (doesLineSegmentIntersectPlane(l, plane))
            {
                avg::Vector2f intersection = getLinePlaneIntersection(l, plane);

                result.push_back(v1);
                result.emplace_back(v1[0], v1[1], v1[2], v1[3],
                        intersection[0], intersection[1]);
            }
        }

        return result;
    }
    */
} // anonymous namespace

namespace avg
{

class SoftMeshDeferred
{
public:
    std::vector<SoftMesh::Vertex> vertices_;
    Brush brush_;
};

SoftMesh::SoftMesh()
{
}

SoftMesh::SoftMesh(std::vector<Vertex>&& vertices, Brush const& brush) :
    deferred_(std::make_shared<SoftMeshDeferred>())
{
    deferred_->vertices_ = std::move(vertices);
    deferred_->brush_ = brush;
}

SoftMesh::SoftMesh(std::vector<Vertex> const& vertices, Brush const& brush) :
    deferred_(std::make_shared<SoftMeshDeferred>())
{
    deferred_->vertices_ = vertices;
    deferred_->brush_ = brush;
}

SoftMesh::~SoftMesh()
{
}

std::vector<SoftMesh::Vertex> const& SoftMesh::getVertices() const
{
    if (!d())
    {
        static std::vector<Vertex> v;
        return v;
    }

    return d()->vertices_;
}

Brush const& SoftMesh::getBrush() const
{
    if (!d())
    {
        static Brush brush;
        return brush;
    }

    return d()->brush_;
}

Transform const& SoftMesh::getTransform() const
{
    return transform_;
}

/*
SoftMesh SoftMesh::getClipped(Rect const& r) const
{
    return *this;
}
*/

SoftMeshDeferred* SoftMesh::getUnique()
{
    if (deferred_.unique())
        return deferred_.get();

    auto d = std::make_shared<SoftMeshDeferred>();
    d->vertices_ = deferred_->vertices_;
    deferred_ = std::move(d);

    return deferred_.get();
}

} // namespace

