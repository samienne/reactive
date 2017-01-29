#include "softmesh.h"

#include "brush.h"

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

