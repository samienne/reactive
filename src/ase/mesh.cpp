#include "mesh.h"

namespace ase
{

Mesh::Mesh()
{
}

Mesh::Mesh(Aabb&& aabb, VertexBuffer const& vertexBuffer) :
    aabb_(aabb),
    vertexBuffer_(vertexBuffer)
{
}

Mesh::Mesh(Aabb&& aabb, VertexBuffer const& vertexBuffer,
        IndexBuffer const& indexBuffer) :
    aabb_(aabb),
    vertexBuffer_(vertexBuffer),
    indexBuffer_(indexBuffer)
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
    return indexBuffer_;
}

bool Mesh::hasIndexBuffer() const
{
    return indexBuffer_.isEmpty();
}

bool Mesh::operator<(Mesh const& other) const
{
    if (vertexBuffer_ == other.vertexBuffer_)
        return indexBuffer_ < other.indexBuffer_;

    return vertexBuffer_ < other.vertexBuffer_;
}

} // namespace

