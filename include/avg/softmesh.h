#pragma once

#include "transform.h"

#include <btl/visibility.h>

#include <vector>
#include <memory>
#include <array>

namespace avg
{
    class SoftMeshDeferred;
    class Brush;

    class BTL_VISIBLE SoftMesh
    {
    public:
        using Vertex = std::array<float, 6>;
        SoftMesh();
        SoftMesh(std::vector<Vertex>&& vertices, Brush const& brush);
        SoftMesh(std::vector<Vertex> const& vertices, Brush const& brush);
        SoftMesh(SoftMesh const&) = default;
        SoftMesh(SoftMesh&&) = default;

        ~SoftMesh();

        SoftMesh& operator=(SoftMesh const&) = default;
        SoftMesh& operator=(SoftMesh&&) = default;

        std::vector<Vertex> const& getVertices() const;
        Brush const& getBrush() const;
        Transform const& getTransform() const;

    private:
        inline SoftMeshDeferred const* d() const { return deferred_.get(); }
        SoftMeshDeferred* getUnique();

        Transform transform_;
        std::shared_ptr<SoftMeshDeferred> deferred_;
    };
}

