#include "renderable.h"

#include "mesh.h"
#include "material.h"
#include "aabb.h"

namespace ase
{

Renderable::Renderable(Mesh mesh, Material material) :
    mesh_(std::move(mesh)),
    material_(std::move(material))
{
}

Renderable::~Renderable()
{
}

Mesh const& Renderable::getMesh() const
{
    return mesh_;
}

Material const& Renderable::getMaterial() const
{
    return material_;
}

Aabb const& Renderable::getAabb() const
{
    return mesh_.getAabb();
}

} // namespace

