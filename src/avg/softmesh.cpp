#include "softmesh.h"

#include "brush.h"
#include "vector.h"

#include <pmr/shared_ptr.h>

namespace avg
{

class SoftMeshDeferred
{
public:
    SoftMeshDeferred(pmr::memory_resource* memory);
    pmr::vector<SoftMesh::Vertex> vertices_;
    Brush brush_;
};

SoftMeshDeferred::SoftMeshDeferred(pmr::memory_resource* memory) :
    vertices_(memory)
{
}

SoftMesh::SoftMesh(pmr::vector<Vertex>&& vertices, Brush const& brush) :
    deferred_(pmr::make_shared<SoftMeshDeferred>(
                vertices.get_allocator().resource(),
                vertices.get_allocator().resource()
                ))
{
    deferred_->vertices_ = std::move(vertices);
    deferred_->brush_ = brush;
}

SoftMesh::SoftMesh(pmr::vector<Vertex> const& vertices, Brush const& brush) :
    deferred_(pmr::make_shared<SoftMeshDeferred>(
                vertices.get_allocator().resource(),
                vertices.get_allocator().resource()
                ))
{
    deferred_->vertices_ = vertices;
    deferred_->brush_ = brush;
}

SoftMesh::~SoftMesh()
{
}

pmr::memory_resource* SoftMesh::getResource() const
{
    return d()->vertices_.get_allocator().resource();
}

pmr::vector<SoftMesh::Vertex> const& SoftMesh::getVertices() const
{
    return d()->vertices_;
}

Brush const& SoftMesh::getBrush() const
{
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

    auto d = pmr::make_shared<SoftMeshDeferred>(getResource(), getResource());
    d->vertices_ = deferred_->vertices_;
    deferred_ = std::move(d);

    return deferred_.get();
}

} // namespace

