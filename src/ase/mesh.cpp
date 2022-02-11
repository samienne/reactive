#include "mesh.h"

namespace ase
{

Mesh::Mesh(Aabb const& aabb, VertexBuffer vertexBuffer) :
    aabb_(aabb),
    vertexBuffer_(std::move(vertexBuffer))
{
}

Mesh::Mesh(Aabb const& aabb, VertexBuffer vertexBuffer,
        IndexBuffer indexBuffer) :
    aabb_(aabb),
    vertexBuffer_(std::move(vertexBuffer)),
    indexBuffer_(std::make_optional(std::move(indexBuffer)))
{
}

Mesh::~Mesh()
{
}

Aabb const& Mesh::getAabb() const
{
    return aabb_;
}

VertexBuffer const& Mesh::getVertexBuffer() const
{
    return vertexBuffer_;
}

IndexBuffer const& Mesh::getIndexBuffer() const
{
    return *indexBuffer_;
}

bool Mesh::hasIndexBuffer() const
{
    return indexBuffer_.has_value();
}

bool Mesh::operator<(Mesh const& other) const
{
    if (vertexBuffer_ == other.vertexBuffer_)
        return indexBuffer_ < other.indexBuffer_;

    return vertexBuffer_ < other.vertexBuffer_;
}

} // namespace

