#pragma once

#include "transform.h"
#include "rect.h"
#include "avgvisibility.h"

#include <vector>
#include <memory>
#include <array>

namespace avg
{
    class SoftMeshDeferred;
    class Brush;

    class AVG_EXPORT SoftMesh
    {
    public:
        using Vertex = std::array<float, 2>;
        SoftMesh();
        SoftMesh(std::vector<Vertex>&& vertices, Brush const& brush);
        SoftMesh(std::vector<Vertex> const& vertices, Brush const& brush);
        SoftMesh(SoftMesh const&) = default;
        SoftMesh(SoftMesh&&) noexcept = default;

        ~SoftMesh();

        SoftMesh& operator=(SoftMesh const&) = default;
        SoftMesh& operator=(SoftMesh&&) noexcept = default;

        std::vector<Vertex> const& getVertices() const;
        Brush const& getBrush() const;
        Transform const& getTransform() const;

        //SoftMesh getClipped(Rect const& r) const;

        friend inline SoftMesh operator*(avg::Transform const& t, SoftMesh mesh)
        {
            mesh.transform_ = t * mesh.transform_;
            return mesh;
        }

    private:
        inline SoftMeshDeferred const* d() const { return deferred_.get(); }
        SoftMeshDeferred* getUnique();

        Transform transform_;
        std::shared_ptr<SoftMeshDeferred> deferred_;
    };
}

