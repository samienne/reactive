#pragma once

#include "mesh.h"
#include "material.h"
#include "matrix.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class Aabb;

    class ASE_EXPORT Renderable
    {
    public:
        Renderable(Mesh mesh, Material material);
        ~Renderable();

        Mesh const& getMesh() const;
        Material const& getMaterial() const;
        Aabb const& getAabb() const;

    private:
        Mesh mesh_;
        Material material_;
    };
}

