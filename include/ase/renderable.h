#pragma once

#include "mesh.h"
#include "material.h"
#include "matrix.h"

#include <memory>

namespace ase
{
    class Aabb;

    class Renderable
    {
    public:
        Renderable();
        Renderable(Mesh const& mesh, Material const& material);
        ~Renderable();

        Mesh const& getMesh() const;
        Material const& getMaterial() const;
        Aabb const& getAabb() const;

    private:
        Mesh mesh_;
        Material material_;
    };
}

